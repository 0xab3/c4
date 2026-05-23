#include "lexer.h"
#include "../nob.h"
#include "common.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>

char const *cxl_token_kind_to_string(enum CX_TokenKind kind) {
  switch (kind) {
    case CX_TOKEN_CURLY_OPEN    : return "TOKEN({)";
    case CX_TOKEN_CURLY_CLOSE   : return "TOKEN(})";
    case CX_TOKEN_PAREN_OPEN    : return "TOKEN(()";
    case CX_TOKEN_PAREN_CLOSE   : return "TOKEN())";
    case CX_TOKEN_SEMI          : return "TOKEN(;)";
    case CX_TOKEN_COLON         : return "TOKEN(:)";
    case CX_TOKEN_COMMA         : return "TOKEN(,)";
    case CX_TOKEN_DOT           : return "TOKEN(.)";
    case CX_TOKEN_NOT           : return "TOKEN(!)";
    case CX_TOKEN_POINTER       : return "TOKEN(^)";
    case CX_TOKEN_ASS           : return "TOKEN(=)";
    case CX_TOKEN_ADD           : return "TOKEN(+)";
    case CX_TOKEN_SUB           : return "TOKEN(-)";
    case CX_TOKEN_MUL           : return "TOKEN(*)";
    case CX_TOKEN_DIV           : return "TOKEN(/)";
    case CX_TOKEN_GT            : return "TOKEN(>)";
    case CX_TOKEN_LT            : return "TOKEN(<)";
    case CX_TOKEN_ARROW         : return "TOKEN(->)";
    case CX_TOKEN_SUBASS        : return "TOKEN(-=)";
    case CX_TOKEN_ADDASS        : return "TOKEN(+=)";
    case CX_TOKEN_MULASS        : return "TOKEN(*=)";
    case CX_TOKEN_DIVASS        : return "TOKEN(/=)";
    case CX_TOKEN_EQ            : return "TOKEN(==)";
    case CX_TOKEN_NEQ           : return "TOKEN(!=)";
    case CX_TOKEN_LTEQ          : return "TOKEN(<=)";
    case CX_TOKEN_GTEQ          : return "TOKEN(>=)";
    case CX_TOKEN_PROCDECL      : return "TOKEN(PROC)";
    case CX_TOKEN_VARDEF        : return "TOKEN(LET)";
    case CX_TOKEN_RETURN        : return "TOKEN(RETURN)";
    case CX_TOKEN_IF            : return "TOKEN(IF)";
    case CX_TOKEN_ELSE          : return "TOKEN(ELSE)";
    case CX_TOKEN_WHILE         : return "TOKEN(WHILE)";
    case CX_TOKEN_PLEX          : return "TOKEN(PLEX)";
    case CX_TOKEN_STRING_LITERAL: return "TOKEN(STRING_LIT)";
    case CX_TOKEN_NUMBER        : return "TOKEN(NUMBER)";
    case CX_TOKEN_IDENTIFIER    : return "TOKEN(IDENTIFIER)";
    case CX_TOKEN_EOF           : return "TOKEN(EOF)";
    default                     : CX_UNREACHABLE("unknown token kind %d\n", kind);
  }
}

bool cxl_match_multiple(char const *to_match, char x) {
  while (*to_match) {
    if (*to_match == x) return true;
    to_match++;
  }
  return false;
}

bool cxl_get_multicharacter_symbol(Nob_String_View sv, enum CX_TokenKind *symbol) {
  if (sv.count <= 1) return false;
  for (size_t i = 0; i < NOB_ARRAY_LEN(CXLEX_MULTI_CHARACTER_SYMBOLS); i++) {
    auto it = CXLEX_MULTI_CHARACTER_SYMBOLS[i];
    if (nob_sv_starts_with(sv, nob_sv_from_cstr(it.name))) {
      if (symbol) *symbol = it.kind;
      return true;
    }
  }
  return false;
}
bool cxl_is_symbol(char x) { return cxl_match_multiple("!=*()-+[]\\/.,;'<>^:", x); }

size_t cxl_get_symbol_width(Nob_String_View *sv) {
  assert(cxl_is_symbol(sv->data[0]));
  if (sv->count > 1 && cxl_get_multicharacter_symbol(*sv, nullptr)) return 2;
  return 1;
}

bool cxl_should_stop(char x) {
  if (isspace(x) || cxl_is_symbol(x)) return true;
  return false;
}

bool cxl_is_string_literal(Nob_String_View string) { return string.data[0] == '"'; }

Nob_String_View cxl_sv_chop_by_token(Nob_String_View *sv) {
  *sv = nob_sv_trim(*sv);
  size_t i = 0;

  if (cxl_is_string_literal(*sv)) {
    i = 1;
    while (i < sv->count && sv->data[i] != '"') i++;
    i++;

  } else if (cxl_is_symbol(*sv->data)) {
    i += cxl_get_symbol_width(sv);
  } else
    while (i < sv->count && !cxl_should_stop(sv->data[i])) i += 1;

  Nob_String_View result = nob_sv_from_parts(sv->data, i);

  if (i < sv->count) {
    sv->count -= i;
    sv->data += i;
  } else {
    sv->count -= i;
    sv->data += i;
  }
  return result;
}

bool cxl_get_keyword(Nob_String_View token, enum CX_TokenKind *keyword) {
  for (size_t i = 0; i < NOB_ARRAY_LEN(CXLEX_KEYWORDS); i++) {
    if (nob_sv_starts_with(token, nob_sv_from_cstr(CXLEX_KEYWORDS[i].name))) {
      if (keyword) *keyword = CXLEX_KEYWORDS[i].kind;
      return true;
    }
  }
  return false;
}

struct CX_Token cxl_token_from_string(Nob_String_View string) {

  enum CX_TokenKind symbol;
  auto ok = cxl_get_multicharacter_symbol(string, &symbol);
  if (ok) return CXLEX_TOKEN_MAKE(symbol, none, 0, string);

  ok = cxl_is_symbol(string.data[0]);
  if (ok)
    return CXLEX_TOKEN_MAKE(CX_CAST(enum CX_TokenKind, string.data[0]), none, 0,
                            string);
  // add validation check if the actual thing is a token or nah

  enum CX_TokenKind keyword;
  ok = cxl_get_keyword(string, &keyword);
  if (ok) return CXLEX_TOKEN_MAKE(keyword, none, 0, string);

  ok = cxl_is_string_literal(string);
  if (ok) return CXLEX_TOKEN_MAKE(CX_TOKEN_STRING_LITERAL, literal, string, string);

  // leaves only integer literals now

  return CXLEX_TOKEN_MAKE(CX_TOKEN_IDENTIFIER, identifier, string, string);
}

bool cxl_lex(struct CX_Lexer *lexer, struct CX_SourceFile file) {
  CX_Array(struct CX_Token) tokens = nullptr;

  auto contents = nob_sv_from_cstr(file.contents);
  while (contents.count > 0) {
    auto lexeme = cxl_sv_chop_by_token(&contents);
    if (lexeme.count == 0) continue;
    auto token = cxl_token_from_string(lexeme);
    cx_da_append(tokens, token);
  }

  lexer->tokens = tokens;
  return true;
}

struct CX_Token cxl_token_peek(struct CX_Lexer *lexer) {
  assert(lexer->cursor < cx_da_meta(lexer->tokens)->count);
  return lexer->tokens[lexer->cursor];
}
bool cxl_token_try_peek(struct CX_Lexer *lexer, struct CX_Token *out) {
  auto tokens_meta = cx_da_meta(lexer->tokens);
  if (lexer->cursor < tokens_meta->count && tokens_meta->count > 0) {
    *out = lexer->tokens[lexer->cursor];
    return true;
  }
  return false;
}

void cxl_token_advance(struct CX_Lexer *lexer, size_t by) {
  auto tokens_meta = cx_da_meta(lexer->tokens);
  if (lexer->cursor + by > tokens_meta->count) {
    lexer->cursor = tokens_meta->count;
  } else {
    lexer->cursor += by;
  }
}

bool cxl_token_consume(struct CX_Lexer *lexer, struct CX_Token *out) {
  auto tokens_meta = cx_da_meta(lexer->tokens);
  if (lexer->cursor < tokens_meta->count) {
    *out = lexer->tokens[lexer->cursor++];
    return true;
  }
  return false;
}
