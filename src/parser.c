#include "parser.h"
#include "../our_nob.h"
#include "arena.h"
#include "ast.h"
#include "common.h"
#include "lexer.h"
#include <__stddef_unreachable.h>
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
    {CX_TOKEN_DOT, 18},        {CX_TOKEN_DEREF, 17},      {CX_TOKEN_REF, 17},
    {CX_TOKEN_PAREN_OPEN, 17}, {CX_TOKEN_CAST, 16},       {CX_TOKEN_MUL, 15},
    {CX_TOKEN_DIV, 15},        {CX_TOKEN_ADD, 14},        {CX_TOKEN_SUB, 14},
    {CX_TOKEN_LT, 12},         {CX_TOKEN_GT, 12},         {CX_TOKEN_LTEQ, 12},
    {CX_TOKEN_GTEQ, 12},       {CX_TOKEN_EQ, 11},         {CX_TOKEN_NEQ, 11},
    {CX_TOKEN_NOT, 10},        {CX_TOKEN_ASS, 4},         {CX_TOKEN_ADDASS, 4},
    {CX_TOKEN_SUBASS, 4},      {CX_TOKEN_MULASS, 4},      {CX_TOKEN_DIVASS, 4},
    {CX_TOKEN_COLON, 5},       {CX_TOKEN_COMMA, 0},       {CX_TOKEN_SEMI, 0},
    {CX_TOKEN_CURLY_OPEN, 0},  {CX_TOKEN_CURLY_CLOSE, 0}, {CX_TOKEN_PAREN_CLOSE, 0}};

CX_Expression *cxp_parse_expr(CX_Parser *parser, ssize_t min_precedence);
CX_Expression *cxp_parse_expr_if_else(CX_Parser *parser);
CX_Expression *cxp_parse_expr_while(CX_Parser *parser);
bool cxp_parse_block(CX_Parser *parser, struct CX_Block *block);

bool cxp_expect(struct CX_Lexer *lexer, enum CX_TokenKind expected,
                struct CX_Token *out) {
  struct CX_Token temp = {};
  if (out == nullptr) {
    out = &temp;
  }
  bool ok = cxl_token_try_peek(lexer, out);
  assert(ok);
  if (out->kind == expected) {
    cxl_advance(lexer, 1);
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

CX_OperatorKind cxp_is_operator(CX_TokenKind kind) {
  switch (kind) {
    case CX_TOKEN_GT:
    case CX_TOKEN_DOT:
    case CX_TOKEN_LT:
    case CX_TOKEN_EQ:
    case CX_TOKEN_ASS:
    case CX_TOKEN_ADD:
    case CX_TOKEN_SUB:
    case CX_TOKEN_MUL:
    case CX_TOKEN_DIV  : return CX_OPERATOR_BINARY;

    case CX_TOKEN_DEREF: return CX_OPERATOR_UNARY_POSTFIX;
    case CX_TOKEN_REF  : return CX_OPERATOR_UNARY_PREFIX;

    default            : return CX_OPERATOR_NULL;
  }
}

CX_Expression *cxp_make_binop(CX_Parser *parser, CX_Operator op, CX_Expression *left,
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
CX_Expression *cxp_make_uop(CX_Parser *parser, CX_Operator op, CX_Expression *expr) {
  CX_Expression *unary_expr = arena_alloc(&parser->storage_arena, sizeof(*unary_expr));
  unary_expr->kind = CX_EXPR_UOP;

  unary_expr->_As.uop = (CX_UnaryOperation){
      .op = op,
      .expr = expr,
  };
  return unary_expr;
}

bool cxp_parse_type(CX_Parser *parser, struct CX_Type *type) {
  CX_Lexer *lexer = &parser->lexer;
  struct CX_Token token = {};
  while (cxl_token_try_peek(lexer, &token) && token.kind == CX_TOKEN_REF) {
    cxl_advance(lexer, 1);
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
  cxl_advance(lexer, 1);
  return true;
}

bool cxp_parse_group(CX_Parser *parser, CX_Tuple *exprs, CX_TokenKind start_group,
                     CX_TokenKind end_group) {
  CX_Lexer *lexer = &parser->lexer;
  CX_Token token = {0};
  if (!cxp_expect(lexer, start_group, &token)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, "(", token);
    return false;
  }

  while (cxl_token_peek(lexer).kind != end_group) {
    CX_Expression *expr = cxp_parse_expr(parser, CX_MIN_PRECEDENCE);
    if (!expr) {
      nob_log(NOB_ERROR, "failed to parse expression while parsing tuple!");
    }
    cx_da_append(*exprs, expr);

    struct CX_Token current_token = cxl_token_peek(lexer);
    if (current_token.kind == CX_TOKEN_COMMA) {
      cxl_advance(lexer, 1);
    } else {
      if (current_token.kind != end_group) {
        char const *token_kind_str = cxl_token_kind_to_string(end_group);
        nob_log(NOB_ERROR, "expected ',' or '%s' got '%s'! %s:%d", token_kind_str,
                cxl_token_kind_to_string(current_token.kind), __FILE__, __LINE__);
        cxl_print_token_location(lexer, current_token);
        return false;
      }
    }
  }
  if (!cxp_expect(lexer, end_group, &token)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, ")", token);
    return false;
  }

  return true;
}

bool cxp_parse_tuple(CX_Parser *parser, CX_Tuple *exprs) {
  return cxp_parse_group(parser, exprs, CX_TOKEN_PAREN_OPEN, CX_TOKEN_PAREN_CLOSE);
}
bool cxp_parse_plex_members(CX_Parser *parser, CX_Tuple *exprs) {
  return cxp_parse_group(parser, exprs, CX_TOKEN_CURLY_OPEN, CX_TOKEN_CURLY_CLOSE);
}

CX_Expression *cxp_parse_unit_expr(CX_Parser *parser) {
  CX_Lexer *lexer = &parser->lexer;
  CX_Token token = cxl_token_peek(lexer);
  switch (token.kind) {
    case CX_TOKEN_NUMBER: {
      CX_Expression *expr = arena_alloc(&parser->storage_arena, sizeof(CX_Expression));
      expr->kind = CX_EXPR_NUMBER;
      expr->_As.number = token._As.number;
      cxl_advance(lexer, 1);
      return expr;
    } break;
    case CX_TOKEN_STRING_LITERAL: {
      CX_Expression *expr = arena_alloc(&parser->storage_arena, sizeof(CX_Expression));
      expr->kind = CX_EXPR_LITERAL;
      expr->_As.literal = token._As.literal;
      cxl_advance(lexer, 1);
      return expr;
    } break;

    case CX_TOKEN_REF: { // we are taking reference
      cxl_advance(lexer, 1);
      ssize_t unary_tok_prec = cxp_get_precedence(token.kind);
      CX_Expression *expr = cxp_parse_expr(parser, unary_tok_prec);

      CX_Expression *unary_expr = cxp_make_uop(parser, CX_OP_REF, expr);
      return unary_expr;

    } break;
    case CX_TOKEN_IDENTIFIER: {
      CX_Expression *expr = arena_alloc(&parser->storage_arena, sizeof(CX_Expression));
      Nob_String_View ident = token._As.identifier;

      expr->kind = CX_EXPR_VAR;
      expr->_As.var_name = ident;

      cxl_advance(lexer, 1);
      token = cxl_token_peek(lexer);
      if (token.kind == CX_TOKEN_PAREN_OPEN) {
        CX_Array(CX_Expression *) tuple = nullptr;
        if (!cxp_parse_tuple(parser, &tuple)) {
          nob_log(NOB_ERROR,
                  "failed to parse procedure call parameters for '" SV_Fmt "'\n",
                  SV_Arg(ident));
          return nullptr;
        }

        CX_Expression_ProcCall call = {
            .proc_name = ident,
            .params = tuple,
        };
        expr->kind = CX_EXPR_CALL;
        expr->_As.call = call;
      } else if (token.kind == CX_TOKEN_CURLY_OPEN) {
        CX_Array(CX_Expression *) fields = nullptr;
        if (!cxp_parse_plex_members(parser, &fields)) {
          nob_log(NOB_ERROR,
                  "failed to parse procedure call parameters for '" SV_Fmt "'\n",
                  SV_Arg(ident));
          return nullptr;
        }
      }

      return expr;

    } break;
    case CX_TOKEN_IF   : return cxp_parse_expr_if_else(parser);
    case CX_TOKEN_WHILE: {
      assert(false);
      return cxp_parse_expr_while(parser);
    }
    case CX_TOKEN_CURLY_OPEN:
      CX_Expression *block = arena_alloc(&parser->storage_arena, sizeof(*block));
      block->kind = CX_EXPR_BLOCK;
      if (!cxp_parse_block(parser, &block->_As.block)) {
        nob_log(NOB_ERROR, "failed to parse block body!\n");
        return nullptr;
      }
      return block;
    case CX_TOKEN_PAREN_OPEN: {
      cxl_advance(lexer, 1);
      CX_Expression *expr = cxp_parse_expr(parser, CX_MIN_PRECEDENCE);

      CX_Token cur_tok = {0};
      if (!cxp_expect(lexer, CX_TOKEN_PAREN_CLOSE, &cur_tok)) {
        // @note we can actually use this as a tuple if we get expect paren close
        // or comma but that will make the entire thing too abstract imo so i don't
        // think we should generalize this

        CX_LOG_UNEXPECTED_TOKEN(lexer, ")", cur_tok);
        return nullptr;
      }
      return expr;
    } break;
    default: {
      printf("failed because yk\n");
      cx_breakpoint();
    }
  }
  return nullptr;
}

CX_Expression *cxp_parse_binop_increasing_precendence(CX_Parser *parser,
                                                      struct CX_Expression *left,
                                                      ssize_t precedence);
CX_Expression *cxp_parse_binop_increasing_precendence(CX_Parser *parser,
                                                      struct CX_Expression *left,
                                                      ssize_t min_prec) {
  CX_Lexer *lexer = &parser->lexer;
  CX_Token token = cxl_token_peek(lexer);
  CX_OperatorKind op_kind = cxp_is_operator(token.kind);

  switch (op_kind) {
    case CX_OPERATOR_BINARY: {
      ssize_t next_prec = cxp_get_precedence(token.kind);
      if (next_prec <= min_prec) { // if precedence is not increasing
        return left;
      } else {
        cxl_advance(lexer, 1);
        CX_Expression *right_expr = cxp_parse_expr(parser, next_prec);
        if (!right_expr) {
          nob_log(NOB_ERROR, "failed to parse binary operation!\n");
          cxl_print_token_location(lexer, cxl_token_peek(lexer));
          return nullptr;
        }
        return cxp_make_binop(parser, (CX_Operator)token.kind, left, right_expr);
      }
    } break;
    case CX_OPERATOR_UNARY_PREFIX: {
      // this should be taken care by unit expression parser
      CX_UNREACHABLE("");
    } break;
    case CX_OPERATOR_UNARY_POSTFIX: {
      ssize_t next_prec = cxp_get_precedence(token.kind);
      if (next_prec <= min_prec) {
        return left;
      } else {
        cxl_advance(lexer, 1);
        assert(token.kind == CX_CAST(CX_TokenKind, CX_OP_DEREF));
        CX_Expression *uop_expr = cxp_make_uop(parser, CX_OP_DEREF, left);
        return uop_expr;
      }
    } break;
    case CX_OPERATOR_NULL: return left;
  }
}

CX_Expression *cxp_parse_expr(CX_Parser *parser, ssize_t min_precedence) {
  CX_Expression *lhs = cxp_parse_unit_expr(parser);

  while (true) {
    CX_Expression *node =
        cxp_parse_binop_increasing_precendence(parser, lhs, min_precedence);
    if (node == lhs) break;
    lhs = node;
  }
  return lhs;
}

CX_Expression *cxp_parse_expr_while(CX_Parser *parser) {
  CX_Lexer *lexer = &parser->lexer;
  CX_Expression *expr = arena_alloc(&parser->storage_arena, sizeof(*expr));
  assert(cxp_expect(lexer, CX_TOKEN_WHILE, nullptr));

  CX_Expression *condition = cxp_parse_expr(parser, CX_MIN_PRECEDENCE);
  if (condition == nullptr) {
    nob_log(NOB_ERROR, "failed to parse condition for while loop!");
    cxl_print_token_location(lexer, cxl_token_peek(lexer));
    return nullptr;
  }

  CX_Expression *body = cxp_parse_expr(parser, CX_MIN_PRECEDENCE);
  if (body == nullptr) {
    nob_log(NOB_ERROR, "failed to parse body of if condition!");
    cxl_print_token_location(lexer, cxl_token_peek(lexer));
    return nullptr;
  }
  CX_Token got = {0};
  if (body->kind != CX_EXPR_BLOCK && !cxp_expect(lexer, CX_TOKEN_SEMI, &got)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, ";", got);
    return nullptr;
  }

  expr->kind = CX_EXPR_WHILE;
  expr->_As.while_.condition = condition;
  expr->_As.while_.then = body;
  return expr;
}
CX_Expression *cxp_parse_expr_if_else(CX_Parser *parser) {
  CX_Lexer *lexer = &parser->lexer;
  CX_Expression *expr = arena_alloc(&parser->storage_arena, sizeof(*expr));
  assert(cxp_expect(lexer, CX_TOKEN_IF, nullptr));
  CX_Expression *condition = cxp_parse_expr(parser, CX_MIN_PRECEDENCE);
  if (condition == nullptr) {
    nob_log(NOB_ERROR, "failed to parse condition for if condition!");
    cxl_print_token_location(lexer, cxl_token_peek(lexer));
    return nullptr;
  }

  CX_Expression *body = cxp_parse_expr(parser, CX_MIN_PRECEDENCE);
  if (body == nullptr) {
    nob_log(NOB_ERROR, "failed to parse body of if condition!");
    cxl_print_token_location(lexer, cxl_token_peek(lexer));
    return nullptr;
  }
  CX_Token got = {0};
  if (body->kind != CX_EXPR_BLOCK && !cxp_expect(lexer, CX_TOKEN_SEMI, &got)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, ";", got);
    return nullptr;
  }
  CX_Expression *else_ = nullptr;
  if (cxl_token_peek(lexer).kind == CX_TOKEN_ELSE) {
    cxl_advance(lexer, 1);
    else_ = cxp_parse_expr(parser, CX_MIN_PRECEDENCE);
    if (else_ == nullptr) {
      nob_log(NOB_ERROR, "failed to parse else body!");
      cxl_print_token_location(lexer, cxl_token_peek(lexer));
      return nullptr;
    }

    if (else_->kind != CX_EXPR_BLOCK && else_->kind != CX_EXPR_IF &&
        !cxp_expect(lexer, CX_TOKEN_SEMI, &got)) {
      CX_LOG_UNEXPECTED_TOKEN(lexer, ";", got);
      return nullptr;
    }
  }
  expr->kind = CX_EXPR_IF;
  expr->_As.if_.condition = condition;
  expr->_As.if_.then = body;
  expr->_As.if_.else_ = else_;
  return expr;
}

bool cxp_parse_stmt_vardef(CX_Parser *parser, CX_Statement *stmt) {
  CX_Lexer *lexer = &parser->lexer;
  CX_Token name = {0};
  CX_Type var_type = {0};
  CX_Expression *value = nullptr;

  cxl_advance(lexer, 1);

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

  value = cxp_parse_expr(parser, CX_MIN_PRECEDENCE);
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

      CX_Expression *expr = cxp_parse_expr(parser, CX_MIN_PRECEDENCE);
      stmt->_As.expr = expr;
      if (expr == nullptr) return false;

      if (!cxp_expect(lexer, CX_TOKEN_SEMI, &token)) {
        CX_LOG_UNEXPECTED_TOKEN(lexer, ";", token);
        return false;
      }

      return true;

    } break;
    case CX_TOKEN_IF: {
      stmt->kind = CX_STMT_EXPR;
      CX_Expression *expr = cxp_parse_expr_if_else(parser);
      if (expr == nullptr) {
        nob_log(NOB_ERROR, "failed to parse if expression!\n");
        return false;
      }
      stmt->_As.expr = expr;
      return true;
    } break;
    case CX_TOKEN_WHILE: {
      stmt->kind = CX_STMT_EXPR;
      CX_Expression *expr = cxp_parse_expr_while(parser);
      if (expr == nullptr) {
        nob_log(NOB_ERROR, "failed to parse while expression!\n");
        return false;
      }
      stmt->_As.expr = expr;
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
bool cxp_parse_block(CX_Parser *parser, struct CX_Block *block) {
  CX_Lexer *lexer = &parser->lexer;
  CX_Token token = {0};

  if (!cxp_expect(lexer, CX_TOKEN_CURLY_OPEN, &token)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, "{", token);
    return false;
  }

  while (cxl_token_peek(lexer).kind != CX_TOKEN_CURLY_CLOSE) {
    CX_Statement stmt;

    if (!cxp_parse_stmt(parser, &stmt)) {
      nob_log(NOB_ERROR, "failed to parse statement!");
      return false;
    }
    cx_da_append(block->stmts, stmt);
  }

  if (!cxp_expect(lexer, CX_TOKEN_CURLY_CLOSE, &token)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, "}", token);
    return false;
  }
  return true;
}

bool cxp_parse_varname_and_type(CX_Parser *parser, CX_VarAndType *x) {
  CX_Lexer *lexer = &parser->lexer;

  struct CX_Token name = {};
  struct CX_Type type = {};
  CX_Token got = {0};

  if (!cxp_expect(lexer, CX_TOKEN_IDENTIFIER, &name)) {
    nob_log(NOB_ERROR, "expected identifier got '%s'!",
            cxl_token_kind_to_string(name.kind));
    cxl_print_token_location(lexer, name);
    return false;
  }
  if (!cxp_expect(lexer, CX_TOKEN_COLON, &got)) {
    nob_log(NOB_ERROR, "expected ':' got '%s'!", cxl_token_kind_to_string(got.kind));
    cxl_print_token_location(lexer, got);
    return false;
  }

  if (!cxp_parse_type(parser, &type)) {
    nob_log(NOB_ERROR, "failed to parse type!\n");
    return false;
  }

  CX_Argument argument = {
      .name = name._As.identifier,
      .type = type,
  };
  *x = argument;
  return true;
}

enum CX_ParseError cxp_parse_proc_args(CX_Parser *parser,
                                       CX_Array(CX_Argument) * tokens) {

  CX_Lexer *lexer = &parser->lexer;
  CX_Token got = {0};

  if (!cxp_expect(lexer, CX_TOKEN_PAREN_OPEN, &got)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, "(", got);
    return CXPE_UNEXPECTED_TOKEN;
  }

  while (cxl_token_peek(lexer).kind != CX_TOKEN_PAREN_CLOSE) {
    CX_Argument argument = {0};
    bool ok = cxp_parse_varname_and_type(parser, &argument);
    if (!ok) {
      nob_log(NOB_ERROR, "failed to parse arguments to procedure call!");
    }
    cx_da_append(*tokens, argument);

    struct CX_Token current_token = cxl_token_peek(lexer);
    if (current_token.kind == CX_TOKEN_COMMA) {
      cxl_advance(lexer, 1);
    } else {
      if (current_token.kind != CX_TOKEN_PAREN_CLOSE) {
        CX_LOG_UNEXPECTED_TOKEN(lexer, ",' or ')", current_token);
        return CXPE_UNEXPECTED_TOKEN;
      }
    }
  }
  if (!cxp_expect(lexer, CX_TOKEN_PAREN_CLOSE, &got)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, ")", got);
    return CXPE_UNEXPECTED_TOKEN;
  }

  return CXPE_OK;
}

bool cxp_parse_plex_def(CX_Parser *parser, CX_Plex *plex) {
  CX_Lexer *lexer = &parser->lexer;
  assert(cxp_expect(lexer, CX_TOKEN_PLEX, nullptr)); // basically unreachable

  struct CX_Token plex_name_tok = {0};
  struct CX_Token got = {0};
  if (!cxp_expect(lexer, CX_TOKEN_IDENTIFIER, &plex_name_tok)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, "plex name", plex_name_tok);
    return false;
  }
  plex->name = plex_name_tok._As.identifier;

  if (!cxp_expect(lexer, CX_TOKEN_CURLY_OPEN, &got)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, "{", got);
    return false;
  }

  while (cxl_token_peek(lexer).kind != CX_TOKEN_CURLY_CLOSE) {
    CX_PlexField fld = {0};
    bool ok = cxp_parse_varname_and_type(parser, &fld);
    if (!ok) {
      nob_log(NOB_ERROR, "failed to parse plex field!");
      return false;
    }
    if (!cxp_expect(lexer, CX_TOKEN_SEMI, &got)) {
      CX_LOG_UNEXPECTED_TOKEN(lexer, ";", got);
      return false;
    }

    cx_da_append(plex->fields, fld);
  }
  assert(cxp_expect(lexer, CX_TOKEN_CURLY_CLOSE, nullptr));
  return true;
}
bool cxp_parse_proc(CX_Parser *parser, struct CX_Procedure *proc, bool *has_body) {
  *has_body = false;
  CX_Lexer *lexer = &parser->lexer;
  assert(cxp_expect(lexer, CX_TOKEN_PROCDECL, nullptr)); // basically unreachable

  struct CX_Token got_token = {};
  struct CX_Token proc_name_tok = {};

  if (!cxp_expect(lexer, CX_TOKEN_IDENTIFIER, &proc_name_tok)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, "procedure name", proc_name_tok);
    return false;
  }

  CX_Array(CX_Argument) args = nullptr;
  CX_ParseError err = cxp_parse_proc_args(parser, &args);
  if (err) return err;

  if (!cxp_expect(lexer, CX_TOKEN_ARROW, &got_token)) {
    CX_LOG_UNEXPECTED_TOKEN(lexer, "->", got_token);
    return false;
  }

  struct CX_Type return_type = {0};
  if (!cxp_parse_type(parser, &return_type)) {
    nob_log(NOB_ERROR, "failed to parse return type for procedure '" SV_Fmt "'!\n",
            SV_Arg(proc_name_tok._As.identifier));
    return false;
  }

  *proc = (struct CX_Procedure){.decl = {
                                    .name = proc_name_tok._As.identifier,
                                    .args = args,
                                    .return_type = return_type,
                                }};

  struct CX_Token token = cxl_token_peek(lexer);
  if (token.kind == CX_TOKEN_SEMI) {
    cxl_advance(lexer, 1);
    return true;
  } else {
    got_token = cxl_token_peek(lexer);
    if (got_token.kind != CX_TOKEN_CURLY_OPEN) {
      CX_LOG_UNEXPECTED_TOKEN(lexer, "}", got_token);
      return false;
    }
  }

  *has_body = true;
  return cxp_parse_block(parser, &proc->block);
}
void cxp_parser_init(CX_Parser *parser, struct CX_Lexer lexer) {
  *parser = (CX_Parser){
      .lexer = lexer,
  };
}
CX_Module *cxp_parse(CX_Parser *parser) {
  CX_Module *mod = &parser->mod;
  CX_Lexer *lexer = &parser->lexer;
  struct CX_Token token;
  bool token_ok = true;
  while ((token_ok = cxl_token_try_peek(lexer, &token))) {
    switch (token.kind) {
      case CX_TOKEN_PROCDECL: {
        struct CX_Procedure proc = {};
        bool has_body = false;
        bool ok = cxp_parse_proc(parser, &proc, &has_body);
        if (!ok) {
          printf("failed to parse procedure!\n");
          CX_Token current_token = cxl_token_peek(&parser->lexer);
          cxl_print_token_location(&parser->lexer, current_token);
          return nullptr;
        }

        if (has_body) {
          cx_da_append(mod->procs, proc);
        } else {
          cx_da_append(mod->proc_decls, proc.decl);
        }
      } break;
      case CX_TOKEN_PLEX: {
        CX_Plex plex = {0};
        bool ok = cxp_parse_plex_def(parser, &plex);
        if (!ok) {
          printf("failed to parse plex!\n");
          CX_Token current_token = cxl_token_peek(&parser->lexer);
          cxl_print_token_location(&parser->lexer, current_token);
          return nullptr;
        }

        if (cxl_token_peek(lexer).kind == CX_TOKEN_SEMI) cxl_advance(lexer, 1);
      } break;
      default: nob_log(NOB_ERROR, "failed to yk"); cx_breakpoint();
    }
  }
  return mod;
}
