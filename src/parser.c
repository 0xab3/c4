#include "../nob.h"
#include "ast.h"
#include "common.h"
#include "lexer.h"
#include <assert.h>
#include <stdio.h>

bool cxp_expect(struct CX_Lexer *lexer, enum CX_TokenKind expected,
                struct CX_Token *out) {
  struct CX_Token temp = {};
  if (out == nullptr) {
    out = &temp;
  }
  auto ok = cxl_token_try_peek(lexer, out);
  assert(ok);
  if (out->kind == expected) {
    cxl_token_advance(lexer, 1);
    return true;
  }
  return false;
}

enum CX_ParseError cxp_parse_type(struct CX_Lexer *lexer, struct CX_Type *type) {
  struct CX_Token token = {};
  while (cxl_token_try_peek(lexer, &token) && token.kind == CX_TOKEN_POINTER) {
    cxl_token_advance(lexer, 1);
    type->ptr_depth++;
  }
  if (token.kind != CX_TOKEN_IDENTIFIER) return CXPE_UNEXPECTED_TOKEN;
  type->inner = token._As.identifier;
  cxl_token_advance(lexer, 1);
  return CXPE_OK;
}
enum CX_ParseError cxp_parse_expr(struct CX_Lexer *lexer, struct CX_Expression *expr) {
  return CXPE_OK;
}
enum CX_ParseError cxp_parse_block(struct CX_Lexer *lexer, struct CX_Block *block) {
  return CXPE_OK;
}

enum CX_ParseError cxp_parse_proc_args(struct CX_Lexer *lexer,
                                       CX_Array(struct CX_Argument) * tokens) {

  auto ok = cxp_expect(lexer, CX_TOKEN_PAREN_OPEN, nullptr);
  if (!ok) return CXPE_UNEXPECTED_TOKEN;

  while (cxl_token_peek(lexer).kind != CX_TOKEN_PAREN_CLOSE) {
    auto temp_token = cxl_token_peek(lexer);
    struct CX_Token name = {};
    struct CX_Type type = {};
    auto ok = cxp_expect(lexer, CX_TOKEN_IDENTIFIER, &name);
    if (!ok) return CXPE_UNEXPECTED_TOKEN;

    ok = cxp_expect(lexer, CX_TOKEN_COLON, nullptr);
    if (!ok) return CXPE_UNEXPECTED_TOKEN;

    auto err = cxp_parse_type(lexer, &type);
    if (err != CXPE_OK) return err;

    struct CX_Argument argument = {
        .name = name._As.identifier,
        .type = type,
    };
    cx_da_append(*tokens, argument);

    auto current_token = cxl_token_peek(lexer);
    if (current_token.kind == CX_TOKEN_COMMA ||
        current_token.kind == CX_TOKEN_PAREN_CLOSE) {
      cxl_token_advance(lexer, 1);
    } else {
      return CXPE_UNEXPECTED_TOKEN;
    }
  }
  return CXPE_OK;
}
enum CX_ParseError cxp_parse_proc(struct CX_Lexer *lexer, struct CX_Procedure *proc) {
  auto ok = cxp_expect(lexer, CX_TOKEN_PROCDECL, nullptr);
  if (!ok) return CXPE_UNEXPECTED_TOKEN;

  struct CX_Token proc_name_tok = {};
  ok = cxp_expect(lexer, CX_TOKEN_IDENTIFIER, &proc_name_tok);
  if (!ok) return CXPE_UNEXPECTED_TOKEN;

  CX_Array(struct CX_Argument) args = nullptr;
  cxp_parse_proc_args(lexer, &args);
  ok = cxp_expect(lexer, CX_TOKEN_ARROW, nullptr);
  if (!ok) return CXPE_UNEXPECTED_TOKEN;

  struct CX_Type return_type = {};
  auto err = cxp_parse_type(lexer, &return_type);
  if (err != CXPE_OK) return err;

  *proc = (struct CX_Procedure){.decl = {
                                    .name = proc_name_tok._As.identifier,
                                    .args = args,
                                    .has_body = false,
                                    .return_type = return_type,
                                }};

  auto token = cxl_token_peek(lexer);
  if (token.kind == CX_TOKEN_SEMI) {
    cxl_token_advance(lexer, 1);
    return CXPE_OK;
  }

  proc->decl.has_body = true;

  return cxp_parse_block(lexer, &proc->block);
}
void cxp_parse(struct CX_Lexer *lexer) {
  struct CX_Token token;
  auto token_ok = true;
  while (token_ok) {
    token_ok = cxl_token_try_peek(lexer, &token);
    if (token.kind != CX_TOKEN_IDENTIFIER && token.kind != CX_TOKEN_STRING_LITERAL) {
      nob_log(NOB_INFO, "token -> %s\n", cxl_token_kind_to_string(token.kind));
    } else {
      nob_log(NOB_INFO, "token -> %s '" SV_Fmt "'\n",
              cxl_token_kind_to_string(token.kind), SV_Arg(token._As.literal));
    }

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
        auto err = cxp_parse_proc(lexer, &proc);
        if (err != CXPE_OK) {
          printf("failed to parse procedure %d\n", err);
          exit(1);
        }
        __asm__("int3");
        printf("smth to not go to braek\n");
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
    }
  }
}
