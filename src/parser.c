#include "parser.h"
#include "../our_nob.h"
#include "arena.h"
#include "ast.h"
#include "common.h"
#include "lexer.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
typedef struct {
  CX_TokenKind kind;
  int prec;
} CX_TokenPrecedence;

static const CX_TokenPrecedence TOKEN_PRECEDENCES[] = {
    {CX_TOKEN_DOT, 17},        {CX_TOKEN_POINTER, 17},   {CX_TOKEN_PAREN_OPEN, 17},
    {CX_TOKEN_CAST, 16},       {CX_TOKEN_MUL, 15},       {CX_TOKEN_DIV, 15},
    {CX_TOKEN_ADD, 14},        {CX_TOKEN_SUB, 14},       {CX_TOKEN_LT, 12},
    {CX_TOKEN_GT, 12},         {CX_TOKEN_LTEQ, 12},      {CX_TOKEN_GTEQ, 12},
    {CX_TOKEN_EQ, 11},         {CX_TOKEN_NEQ, 11},       {CX_TOKEN_NOT, 10},
    {CX_TOKEN_ASS, 4},         {CX_TOKEN_ADDASS, 4},     {CX_TOKEN_SUBASS, 4},
    {CX_TOKEN_MULASS, 4},      {CX_TOKEN_DIVASS, 4},     {CX_TOKEN_COLON, 5},
    {CX_TOKEN_COMMA, 0},       {CX_TOKEN_SEMI, 0},       {CX_TOKEN_CURLY_OPEN, 0},
    {CX_TOKEN_CURLY_CLOSE, 0}, {CX_TOKEN_PAREN_CLOSE, 0}};

CX_Expression *cxp_parse_expr(CX_Parser *parser, ssize_t min_precedence);

bool cxp_expect(struct CX_Lexer *lexer, enum CX_TokenKind expected,
                struct CX_Token *out) {
  struct CX_Token temp = {};
  if (out == nullptr) {
    out = &temp;
  }
  bool ok = cxl_token_try_peek(lexer, out);
  assert(ok);
  if (out->kind == expected) {
    cxl_token_advance(lexer, 1);
    return true;
  }
  return false;
}

ssize_t cxp_get_precedence(CX_TokenKind kind) {
  for (size_t it_index = 0; it_index < NOB_ARRAY_LEN(TOKEN_PRECEDENCES); it_index++) {
    CX_TokenPrecedence it = TOKEN_PRECEDENCES[it_index];
    if (it.kind == kind) {
      return it.prec;
    }
  }
  CX_UNREACHABLE("well we requested kind that don't have precedence or smth");
}

bool cxp_is_token_binop(CX_TokenKind kind) {
  switch (kind) {
    case CX_TOKEN_GT:
    case CX_TOKEN_ASS:
    case CX_TOKEN_ADD:
    case CX_TOKEN_SUB:
    case CX_TOKEN_MUL:
    case CX_TOKEN_DIV: return true;

    default          : return false;
  }
}

bool cxp_parse_type(CX_Parser *parser, struct CX_Type *type) {
  CX_Lexer *lexer = &parser->lexer;
  struct CX_Token token = {};
  while (cxl_token_try_peek(lexer, &token) && token.kind == CX_TOKEN_POINTER) {
    cxl_token_advance(lexer, 1);
    type->ptr_depth++;
  }
  if (token.kind != CX_TOKEN_IDENTIFIER) {
    // @TODO: this should be change to an expression because we will have typeoef
    nob_log(NOB_ERROR, "expected type_name got '%s'!",
            cxl_token_kind_to_string(token.kind));
    cxl_print_token_location(lexer, token);
    return false;
  }
  type->inner = token._As.identifier;
  cxl_token_advance(lexer, 1);
  return true;
}

bool cxp_parse_tuple(CX_Parser *parser, CX_Array(CX_Expression *) * exprs) {
  CX_Lexer *lexer = &parser->lexer;
  CX_Token token = {0};
  if (!cxp_expect(lexer, CX_TOKEN_PAREN_OPEN, &token)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, "(", token);
    return false;
  }

  while (cxl_token_peek(lexer).kind != CX_TOKEN_PAREN_CLOSE) {
    CX_Expression *expr = cxp_parse_expr(parser, CX_MAX_PRECEDENCE);
    if (!expr) {
      nob_log(NOB_ERROR, "failed to parse expression while parsing tuple!");
    }
    cx_da_append(*exprs, expr);

    struct CX_Token current_token = cxl_token_peek(lexer);
    if (current_token.kind == CX_TOKEN_COMMA) {
      cxl_token_advance(lexer, 1);
    } else {
      if (current_token.kind != CX_TOKEN_PAREN_CLOSE) {
        CX_LOG_UNEXPECTED_TOKEN(lexer, ",' or '(", current_token);
        return false;
      }
    }
  }
  if (!cxp_expect(lexer, CX_TOKEN_PAREN_CLOSE, &token)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, ")", token);
    return false;
  }

  return true;
}

bool cxp_parse_unit_expr(CX_Parser *parser, struct CX_Expression *expr) {
  CX_Lexer *lexer = &parser->lexer;
  CX_Token token = cxl_token_peek(lexer);
  switch (token.kind) {
    case CX_TOKEN_NUMBER: {
      expr->kind = CX_EXPR_NUMBER;
      expr->_As.number = token._As.number;
      cxl_token_advance(lexer, 1);
      return true;
    } break;
    case CX_TOKEN_STRING_LITERAL: {
      expr->kind = CX_EXPR_LITERAL;
      expr->_As.literal = token._As.literal;
      cxl_token_advance(lexer, 1);
      return true;
    } break;
    case CX_TOKEN_IDENTIFIER: {
      Nob_String_View ident = token._As.identifier;

      expr->kind = CX_EXPR_VAR;
      expr->_As.var_name = ident;

      cxl_token_advance(lexer, 1);
      token = cxl_token_peek(lexer);
      if (token.kind == CX_TOKEN_PAREN_OPEN) {
        CX_Array(CX_Expression *) tuple = nullptr;
        if (!cxp_parse_tuple(parser, &tuple)) {
          nob_log(NOB_ERROR,
                  "failed to parse procedure call parameters for '" SV_Fmt "'\n",
                  SV_Arg(ident));
          return false;
        }

        CX_Expression_ProcCall call = {
            .proc_name = ident,
            .params = tuple,
        };
        expr->kind = CX_EXPR_CALL;
        expr->_As.call = call;
      }

      return true;

    } break;
    default: {
      printf("failed because yk\n");
    }
  }
  return false;
}

CX_Expression *cxp_parse_binop_increasing_precendence(CX_Parser *parser,
                                                      struct CX_Expression *left,
                                                      ssize_t precedence);
CX_Expression *cxp_make_binop(CX_Parser *parser, CX_Operand op, CX_Expression *left,
                              CX_Expression *right) {
  CX_Expression *ret = arena_alloc(&parser->storage_arena, sizeof(CX_Expression));
  ret->kind = CX_EXPR_BINOP;
  ret->_As.binop = (CX_BinaryOperation){
      .op = op,
      .left = left,
      .right = right,
  };
  return ret;
}
CX_Expression *cxp_parse_binop_increasing_precendence(CX_Parser *parser,
                                                      struct CX_Expression *left,
                                                      ssize_t min_prec) {
  CX_Lexer *lexer = &parser->lexer;
  CX_Token token = cxl_token_peek(lexer);
  if (!cxp_is_token_binop(token.kind)) return left;

  ssize_t next_prec = cxp_get_precedence(token.kind);
  if (next_prec <= min_prec) { // if precedence is not increasing
    return left;
  } else {
    cxl_token_advance(lexer, 1);
    CX_Expression *right_expr = cxp_parse_expr(parser, next_prec);
    if (!right_expr) {
      assert(false);
      nob_log(NOB_ERROR, "failed to parse binary operation!\n");
      cxl_print_token_location(lexer, cxl_token_peek(lexer));
      return nullptr;
    }
    return cxp_make_binop(parser, (CX_Operand)token.kind, left, right_expr);
  }
}

CX_Expression *cxp_parse_expr(CX_Parser *parser, ssize_t min_precedence) {
  CX_Expression *lhs = arena_alloc(&parser->storage_arena, sizeof(*lhs));
  cxp_parse_unit_expr(parser, lhs);

  while (true) {
    CX_Expression *node =
        cxp_parse_binop_increasing_precendence(parser, lhs, min_precedence);
    if (node == lhs) break;
    lhs = node;
  }
  return lhs;
}

bool cxp_parse_stmt_vardef(CX_Parser *parser, CX_Statement *stmt) {
  CX_Lexer *lexer = &parser->lexer;
  CX_Token name = {0};
  CX_Type var_type = {0};
  CX_Expression *value = nullptr;

  cxl_token_advance(lexer, 1);

  if (!cxp_expect(lexer, CX_TOKEN_IDENTIFIER, &name)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, "var_name", name);
    return false;
  }

  CX_Token token = cxl_token_peek(lexer);
  if (token.kind != CX_TOKEN_COLON) {
    goto AFTER_TYPE_PARSING;
  }

  if (!cxp_expect(lexer, CX_TOKEN_COLON, &token)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, ":", token);
    return false;
  }

  if (!cxp_parse_type(parser, &var_type)) {
    nob_log(NOB_ERROR, "failed to parse type!\n");
    return false;
  }

AFTER_TYPE_PARSING:
  if (!cxp_expect(lexer, CX_TOKEN_ASS, &token)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, "=", token);
    return false;
  }

  value = cxp_parse_expr(parser, CX_MAX_PRECEDENCE);
  if (!value) {
    nob_log(NOB_ERROR, "unable to parse expression!");
    return false;
  }

  if (!cxp_expect(lexer, CX_TOKEN_SEMI, &token)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, ";", token);
    return false;
  }

  stmt->kind = CX_STMT_VARDECL;
  stmt->_As.var_decl = (CX_Statement_VarDecl){
      .name = name._As.identifier,
      .type = var_type,
      .value = value,
  };
  return true;
}
bool cxp_parse_stmt(CX_Parser *parser, struct CX_Statement *stmt) {
  CX_Lexer *lexer = &parser->lexer;
  CX_Token token = cxl_token_peek(lexer);
  switch (token.kind) {
    case CX_TOKEN_VARDEF: {
      return cxp_parse_stmt_vardef(parser, stmt);
    } break;
    case CX_TOKEN_IDENTIFIER: {
      stmt->kind = CX_STMT_EXPR;

      CX_Expression *expr = cxp_parse_expr(parser, CX_MAX_PRECEDENCE);
      stmt->_As.expr = expr;
      if (expr == nullptr) return false;

      if (!cxp_expect(lexer, CX_TOKEN_SEMI, &token)) {
        CX_LOG_UNEXPECTED_TOKEN(lexer, ";", token);
        return false;
      }

      return true;

    } break;
    default: {
      nob_log(NOB_ERROR, "failed to parse statement!\n");
      cxl_print_token_location(lexer, cxl_token_peek(lexer));
      return false;
    } break;
  }
  CX_UNREACHABLE("self explanatory");
}
enum CX_ParseError cxp_parse_block(CX_Parser *parser, struct CX_Block *block) {
  CX_Lexer *lexer = &parser->lexer;
  CX_Token token = {0};

  if (!cxp_expect(lexer, CX_TOKEN_CURLY_OPEN, &token)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, "{", token);
    return CXPE_UNEXPECTED_TOKEN;
  }

  while (cxl_token_peek(lexer).kind != CX_TOKEN_CURLY_CLOSE) {
    CX_Statement stmt;

    if (!cxp_parse_stmt(parser, &stmt)) {
      nob_log(NOB_ERROR, "failed to parse statement!");
      return CXPE_UNEXPECTED_TOKEN;
    }
    cx_da_append(block->stmts, stmt);
  }

  if (!cxp_expect(lexer, CX_TOKEN_CURLY_CLOSE, &token)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, "}", token);
    return CXPE_UNEXPECTED_TOKEN;
  }
  return CXPE_OK;
}

enum CX_ParseError cxp_parse_proc_args(CX_Parser *parser,
                                       CX_Array(struct CX_Argument) * tokens) {

  CX_Lexer *lexer = &parser->lexer;
  CX_Token got = {0};

  if (!cxp_expect(lexer, CX_TOKEN_PAREN_OPEN, &got)) {
    nob_log(NOB_ERROR, "expected '(' got '%s'!", cxl_token_kind_to_string(got.kind));
    cxl_print_token_location(lexer, got);
    return CXPE_UNEXPECTED_TOKEN;
  }

  while (cxl_token_peek(lexer).kind != CX_TOKEN_PAREN_CLOSE) {
    struct CX_Token name = {};
    struct CX_Type type = {};

    if (!cxp_expect(lexer, CX_TOKEN_IDENTIFIER, &name)) {
      nob_log(NOB_ERROR, "expected argument name got '%s'!",
              cxl_token_kind_to_string(name.kind));
      cxl_print_token_location(lexer, name);
      return CXPE_UNEXPECTED_TOKEN;
    }

    if (!cxp_expect(lexer, CX_TOKEN_COLON, &got)) {
      nob_log(NOB_ERROR, "expected ':' got '%s'!", cxl_token_kind_to_string(got.kind));
      cxl_print_token_location(lexer, got);
      return CXPE_UNEXPECTED_TOKEN;
    }

    if (!cxp_parse_type(parser, &type)) {
      nob_log(NOB_ERROR, "failed to parse type!\n");
      return CXPE_UNEXPECTED_TOKEN;
    }

    struct CX_Argument argument = {
        .name = name._As.identifier,
        .type = type,
    };
    cx_da_append(*tokens, argument);

    struct CX_Token current_token = cxl_token_peek(lexer);
    if (current_token.kind == CX_TOKEN_COMMA) {
      cxl_token_advance(lexer, 1);
    } else {
      if (current_token.kind != CX_TOKEN_PAREN_CLOSE) {
        CX_LOG_UNEXPECTED_TOKEN(lexer, ",' or '(", current_token);
        return CXPE_UNEXPECTED_TOKEN;
      }
    }
  }
  if (!cxp_expect(lexer, CX_TOKEN_PAREN_CLOSE, &got)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, "0' or '(", got);
    return CXPE_UNEXPECTED_TOKEN;
  }

  return CXPE_OK;
}
enum CX_ParseError cxp_parse_proc(CX_Parser *parser, struct CX_Procedure *proc) {
  CX_Lexer *lexer = &parser->lexer;
  assert(cxp_expect(lexer, CX_TOKEN_PROCDECL, nullptr)); // basically unreachable

  struct CX_Token got_token = {};
  struct CX_Token proc_name_tok = {};

  if (!cxp_expect(lexer, CX_TOKEN_IDENTIFIER, &proc_name_tok)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, "procedure name", proc_name_tok);
    return CXPE_UNEXPECTED_TOKEN;
  }

  CX_Array(struct CX_Argument) args = nullptr;
  CX_ParseError err = cxp_parse_proc_args(parser, &args);
  if (err) return err;

  if (!cxp_expect(lexer, CX_TOKEN_ARROW, &got_token)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, "->", got_token);
    return CXPE_UNEXPECTED_TOKEN;
  }

  struct CX_Type return_type = {0};
  if (!cxp_parse_type(parser, &return_type)) {
    nob_log(NOB_ERROR, "failed to parse return type for procedure '" SV_Fmt "'!\n",
            SV_Arg(proc_name_tok._As.identifier));
    return CXPE_UNEXPECTED_TOKEN;
  }

  *proc = (struct CX_Procedure){.decl = {
                                    .name = proc_name_tok._As.identifier,
                                    .args = args,
                                    .has_body = false,
                                    .return_type = return_type,
                                }};

  struct CX_Token token = cxl_token_peek(lexer);
  if (token.kind == CX_TOKEN_SEMI) {
    cxl_token_advance(lexer, 1);
    return CXPE_OK;
  } else {
    got_token = cxl_token_peek(lexer);
    if (got_token.kind != CX_TOKEN_CURLY_OPEN) {
      CX_LOG_UNEXPECTED_TOKEN(lexer, "}", got_token);
      return CXPE_UNEXPECTED_TOKEN;
    }
  }

  proc->decl.has_body = true;

  return cxp_parse_block(parser, &proc->block);
}
void cxp_parser_new(CX_Parser *parser, struct CX_Lexer lexer) {
  *parser = (CX_Parser){
      .lexer = lexer,
  };
}
void cxp_parse(CX_Parser *parser) {
  CX_Lexer *lexer = &parser->lexer;
  struct CX_Token token;
  bool token_ok = true;
  while ((token_ok = cxl_token_try_peek(lexer, &token))) {
    switch (token.kind) {
      case CX_TOKEN_CURLY_OPEN : CX_UNREACHABLE("CX_TOKEN_CURLY_OPEN");
      case CX_TOKEN_CURLY_CLOSE: CX_UNREACHABLE("CX_TOKEN_CURLY_CLOSE");
      case CX_TOKEN_PAREN_OPEN : CX_UNREACHABLE("CX_TOKEN_PAREN_OPEN");
      case CX_TOKEN_PAREN_CLOSE: CX_UNREACHABLE("CX_TOKEN_PAREN_CLOSE");
      case CX_TOKEN_SEMI       : CX_UNREACHABLE("CX_TOKEN_SEMI");
      case CX_TOKEN_COLON      : CX_UNREACHABLE("CX_TOKEN_COLON");
      case CX_TOKEN_COMMA      : CX_UNREACHABLE("CX_TOKEN_COMMA");
      case CX_TOKEN_DOT        : CX_UNREACHABLE("CX_TOKEN_DOT");
      case CX_TOKEN_NOT        : CX_UNREACHABLE("CX_TOKEN_NOT");
      case CX_TOKEN_POINTER    : CX_UNREACHABLE("CX_TOKEN_POINTER");
      case CX_TOKEN_ASS        : CX_UNREACHABLE("CX_TOKEN_ASS");
      case CX_TOKEN_ADD        : CX_UNREACHABLE("CX_TOKEN_ADD");
      case CX_TOKEN_SUB        : CX_UNREACHABLE("CX_TOKEN_SUB");
      case CX_TOKEN_MUL        : CX_UNREACHABLE("CX_TOKEN_MUL");
      case CX_TOKEN_DIV        : CX_UNREACHABLE("CX_TOKEN_DIV");
      case CX_TOKEN_LT         : CX_UNREACHABLE("CX_TOKEN_LT");
      case CX_TOKEN_GT         : CX_UNREACHABLE("CX_TOKEN_GT");
      case CX_TOKEN_ARROW      : CX_UNREACHABLE("CX_TOKEN_ARROW");
      case CX_TOKEN_SUBASS     : CX_UNREACHABLE("CX_TOKEN_SUBASS");
      case CX_TOKEN_ADDASS     : CX_UNREACHABLE("CX_TOKEN_ADDASS");
      case CX_TOKEN_MULASS     : CX_UNREACHABLE("CX_TOKEN_MULASS");
      case CX_TOKEN_DIVASS     : CX_UNREACHABLE("CX_TOKEN_DIVASS");
      case CX_TOKEN_EQ         : CX_UNREACHABLE("CX_TOKEN_EQ");
      case CX_TOKEN_NEQ        : CX_UNREACHABLE("CX_TOKEN_NEQ");
      case CX_TOKEN_LTEQ       : CX_UNREACHABLE("CX_TOKEN_LTEQ");
      case CX_TOKEN_GTEQ       : CX_UNREACHABLE("CX_TOKEN_GTEQ");
      case CX_TOKEN_PROCDECL   : {
        struct CX_Procedure proc = {};
        enum CX_ParseError err = cxp_parse_proc(parser, &proc);
        if (err != CXPE_OK) {
          printf("failed to parse procedure\n");
          CX_Token current_token = cxl_token_peek(&parser->lexer);
          cxl_print_token_location(&parser->lexer, current_token);
          exit(1);
        }
      } break;
      case CX_TOKEN_VARDEF        : CX_UNREACHABLE("CX_TOKEN_VARDEF");
      case CX_TOKEN_RETURN        : CX_UNREACHABLE("CX_TOKEN_RETURN");
      case CX_TOKEN_IF            : CX_UNREACHABLE("CX_TOKEN_IF");
      case CX_TOKEN_ELSE          : CX_UNREACHABLE("CX_TOKEN_ELSE");
      case CX_TOKEN_WHILE         : CX_UNREACHABLE("CX_TOKEN_WHILE");
      case CX_TOKEN_PLEX          : CX_UNREACHABLE("CX_TOKEN_PLEX");
      case CX_TOKEN_STRING_LITERAL: CX_UNREACHABLE("CX_TOKEN_STRING_LITERAL");
      case CX_TOKEN_NUMBER        : CX_UNREACHABLE("CX_TOKEN_NUMBER");
      case CX_TOKEN_IDENTIFIER    : CX_UNREACHABLE("CX_TOKEN_IDENTIFIER");
      case CX_TOKEN_EOF           : CX_UNREACHABLE("CX_TOKEN_EOF");
      case CX_TOKEN_CAST          : CX_UNREACHABLE("CX_TOKEN_CAST");
      case CX_TOKEN_UNDEFINED     : CX_UNREACHABLE("CX_TOKEN_UNDEFINED");
      case CX_TOKEN_COMMENT       : CX_UNREACHABLE("CX_TOKEN_COMMENT"); ;
    }
  }
}
