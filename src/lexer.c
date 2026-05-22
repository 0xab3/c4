#include "lexer.h"
#include "../nob.h"
#include "common.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>

char const *c4lex_token_kind_to_string(enum C4_TokenKind kind) {
  switch (kind) {
  case C4_TOKEN_CURLY_OPEN:     return "TOKEN({)";
  case C4_TOKEN_CURLY_CLOSE:    return "TOKEN(})";
  case C4_TOKEN_PAREN_OPEN:     return "TOKEN(()";
  case C4_TOKEN_PAREN_CLOSE:    return "TOKEN())";
  case C4_TOKEN_SEMI:           return "TOKEN(;)";
  case C4_TOKEN_COLON:          return "TOKEN(:)";
  case C4_TOKEN_COMMA:          return "TOKEN(,)";
  case C4_TOKEN_DOT:            return "TOKEN(.)";
  case C4_TOKEN_NOT:            return "TOKEN(!)";
  case C4_TOKEN_POINTER:        return "TOKEN(^)";
  case C4_TOKEN_ASS:            return "TOKEN(=)";
  case C4_TOKEN_ADD:            return "TOKEN(+)";
  case C4_TOKEN_SUB:            return "TOKEN(-)";
  case C4_TOKEN_MUL:            return "TOKEN(*)";
  case C4_TOKEN_DIV:            return "TOKEN(/)";
  case C4_TOKEN_GT:             return "TOKEN(>)";
  case C4_TOKEN_LT:             return "TOKEN(<)";
  case C4_TOKEN_ARROW:          return "TOKEN(->)";
  case C4_TOKEN_SUBASS:         return "TOKEN(-=)";
  case C4_TOKEN_ADDASS:         return "TOKEN(+=)";
  case C4_TOKEN_MULASS:         return "TOKEN(*=)";
  case C4_TOKEN_DIVASS:         return "TOKEN(/=)";
  case C4_TOKEN_EQ:             return "TOKEN(==)";
  case C4_TOKEN_NEQ:            return "TOKEN(!=)";
  case C4_TOKEN_LTEQ:           return "TOKEN(<=)";
  case C4_TOKEN_GTEQ:           return "TOKEN(>=)";
  case C4_TOKEN_PROCDECL:       return "TOKEN(PROC)";
  case C4_TOKEN_VARDEF:         return "TOKEN(LET)";
  case C4_TOKEN_RETURN:         return "TOKEN(RETURN)";
  case C4_TOKEN_IF:             return "TOKEN(IF)";
  case C4_TOKEN_ELSE:           return "TOKEN(ELSE)";
  case C4_TOKEN_WHILE:          return "TOKEN(WHILE)";
  case C4_TOKEN_PLEX:           return "TOKEN(PLEX)";
  case C4_TOKEN_STRING_LITERAL: return "TOKEN(STRING_LIT)";
  case C4_TOKEN_NUMBER:         return "TOKEN(NUMBER)";
  case C4_TOKEN_IDENTIFIER:     return "TOKEN(IDENTIFIER)";
  default:                      C4_UNREACHABLE("unknown token kind %d\n", kind);
  }
}

bool c4lex_match_multiple(char const *to_match, char x) {
  while (*to_match) {
    if (*to_match == x) return true;
    to_match++;
  }
  return false;
}

bool c4lex_get_multicharacter_symbol(Nob_String_View sv,
                                     enum C4_TokenKind *symbol) {
  if (sv.count <= 1) return false;
  for (size_t i = 0; i < NOB_ARRAY_LEN(C4LEX_MULTI_CHARACTER_SYMBOLS); i++) {
    auto it = C4LEX_MULTI_CHARACTER_SYMBOLS[i];
    if (nob_sv_starts_with(sv, nob_sv_from_cstr(it.name))) {
      if (symbol) *symbol = it.kind;
      return true;
    }
  }
  return false;
}
bool c4lex_is_symbol(char x) {
  return c4lex_match_multiple("!=*()-+[]\\/.,;'<>^:", x);
}

size_t c4lex_get_symbol_width(Nob_String_View *sv) {
  assert(c4lex_is_symbol(sv->data[0]));
  if (sv->count > 1 && c4lex_get_multicharacter_symbol(*sv, nullptr)) return 2;
  return 1;
}

bool c4lex_should_stop(char x) {
  if (isspace(x) || c4lex_is_symbol(x)) return true;
  return false;
}

bool c4lex_is_string_literal(Nob_String_View string) {
  return string.data[0] == '"';
}

Nob_String_View c4lex_sv_chop_by_token(Nob_String_View *sv) {
  *sv = nob_sv_trim(*sv);
  size_t i = 0;

  if (c4lex_is_string_literal(*sv)) {
    i = 1;
    while (i < sv->count && sv->data[i] != '"') i++;
    i++;

  } else if (c4lex_is_symbol(*sv->data)) {
    i += c4lex_get_symbol_width(sv);
  } else
    while (i < sv->count && !c4lex_should_stop(sv->data[i])) i += 1;

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

bool c4lex_get_keyword(Nob_String_View token, enum C4_TokenKind *keyword) {
  for (size_t i = 0; i < NOB_ARRAY_LEN(C4LEX_KEYWORDS); i++) {
    if (nob_sv_starts_with(token, nob_sv_from_cstr(C4LEX_KEYWORDS[i].name))) {
      if (keyword) *keyword = C4LEX_KEYWORDS[i].kind;
      return true;
    }
  }
  return false;
}

struct C4_Token c4lex_token_from_string(Nob_String_View string) {

  enum C4_TokenKind symbol;
  auto ok = c4lex_get_multicharacter_symbol(string, &symbol);
  if (ok) return C4LEX_TOKEN_MAKE(symbol, none, 0, string);

  ok = c4lex_is_symbol(string.data[0]);
  if (ok)
    return C4LEX_TOKEN_MAKE(C4_CAST(enum C4_TokenKind, string.data[0]), none, 0,
                            string);
  // add validation check if the actual thing is a token or nah

  enum C4_TokenKind keyword;
  ok = c4lex_get_keyword(string, &keyword);
  if (ok) return C4LEX_TOKEN_MAKE(keyword, none, 0, string);

  ok = c4lex_is_string_literal(string);
  if (ok)
    return C4LEX_TOKEN_MAKE(C4_TOKEN_STRING_LITERAL, literal, string, string);

  // leaves only integer literals now

  return C4LEX_TOKEN_MAKE(C4_TOKEN_IDENTIFIER, identifier, string, string);
}

struct C4_Tokens c4lex_parse_token(struct C4_Lexer *lexer,
                                   struct C4_SourceFile file) {
  struct C4_Tokens tokens = {};

  auto contents = nob_sv_from_cstr(file.contents);
  while (contents.count > 0) {
    auto lexeme = c4lex_sv_chop_by_token(&contents);
    if (lexeme.count == 0) continue;
    auto token = c4lex_token_from_string(lexeme);
    nob_da_append(&tokens, token);
  }

  return tokens;
}
