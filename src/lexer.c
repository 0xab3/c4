#include "lexer.h"
#include "../our_nob.h"
#include "common.h"
#include <bits/posix1_lim.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

char const *cxl_token_kind_to_string(enum CX_TokenKind kind) {
  switch (kind) {
    case CX_TOKEN_CURLY_OPEN    : return "{";
    case CX_TOKEN_CURLY_CLOSE   : return "}";
    case CX_TOKEN_PAREN_OPEN    : return "(";
    case CX_TOKEN_PAREN_CLOSE   : return ")";
    case CX_TOKEN_SEMI          : return ";";
    case CX_TOKEN_COLON         : return ":";
    case CX_TOKEN_COMMA         : return ",";
    case CX_TOKEN_DOT           : return ".";
    case CX_TOKEN_NOT           : return "!";
    case CX_TOKEN_POINTER       : return "^";
    case CX_TOKEN_ASS           : return "=";
    case CX_TOKEN_ADD           : return "+";
    case CX_TOKEN_SUB           : return "-";
    case CX_TOKEN_MUL           : return "*";
    case CX_TOKEN_DIV           : return "/";
    case CX_TOKEN_GT            : return ">";
    case CX_TOKEN_LT            : return "<";
    case CX_TOKEN_ARROW         : return "->";
    case CX_TOKEN_SUBASS        : return "-=";
    case CX_TOKEN_ADDASS        : return "+=";
    case CX_TOKEN_MULASS        : return "*=";
    case CX_TOKEN_DIVASS        : return "/=";
    case CX_TOKEN_EQ            : return "==";
    case CX_TOKEN_NEQ           : return "!=";
    case CX_TOKEN_LTEQ          : return "<=";
    case CX_TOKEN_GTEQ          : return ">=";
    case CX_TOKEN_PROCDECL      : return "proc";
    case CX_TOKEN_VARDEF        : return "let";
    case CX_TOKEN_RETURN        : return "return";
    case CX_TOKEN_IF            : return "if";
    case CX_TOKEN_ELSE          : return "else";
    case CX_TOKEN_WHILE         : return "while";
    case CX_TOKEN_PLEX          : return "plex";
    case CX_TOKEN_STRING_LITERAL: return "string_literal";
    case CX_TOKEN_NUMBER        : return "number";
    case CX_TOKEN_IDENTIFIER    : return "identifier";
    case CX_TOKEN_EOF           : return "eof";
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
    struct CX_TokenName it = CXLEX_MULTI_CHARACTER_SYMBOLS[i];
    if (nob_sv_starts_with(sv, nob_sv_from_cstr(it.name))) {
      if (symbol) *symbol = it.kind;
      return true;
    }
  }
  return false;
}
bool cxl_is_symbol(char x) { return cxl_match_multiple("!=*(){}-+[]\\/.,;'<>^:", x); }

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

bool cxl__char_is_number(char x) { return (x >= '0' && x <= '9'); }
CX_Number cxl_string_to_number(Nob_String_View string) {

  assert(cxl__char_is_number(string.data[0]));
  CX_Number number = 0;
  size_t i = 0;
  while (cxl__char_is_number(string.data[i]) && i < string.count) {
    i64 as_int = (i64)(string.data[i] - '0');
    assert(as_int >= 0 && as_int <= 9);
    number = (i64)(number * 10 + as_int);
    i++;
  }
  return number;
}

struct CX_Token cxl_token_from_string(Nob_String_View string) {

  enum CX_TokenKind symbol;
  bool ok = cxl_get_multicharacter_symbol(string, &symbol);
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

  ok = cxl__char_is_number(string.data[0]);
  if (ok) {
    CX_Number as_num = cxl_string_to_number(string);
    return CXLEX_TOKEN_MAKE(CX_TOKEN_NUMBER, number, as_num, string);
  }

  return CXLEX_TOKEN_MAKE(CX_TOKEN_IDENTIFIER, identifier, string, string);
}

void cxl_lexer_init(struct CX_Lexer *lexer, struct CX_SourceFile file) {
  lexer->file = file;
}

bool cxl_lex(struct CX_Lexer *lexer) {
  CX_Array(struct CX_Token) tokens = nullptr;

  Nob_String_View contents = lexer->file.contents;
  while (contents.count > 0) {
    Nob_String_View lexeme = cxl_sv_chop_by_token(&contents);
    if (lexeme.count == 0) continue;
    struct CX_Token token = cxl_token_from_string(lexeme);
    if (token.kind == CX_TOKEN_COMMENT) {
      size_t to_chop = 0;
      while (contents.data[to_chop] != '\r' && contents.data[to_chop] != '\n') {
        to_chop += 1;
      }
      nob_sv_chop_left(&contents, to_chop);
      continue;
    }
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
  struct CX_DArrayMeta *tokens_meta = cx_da_meta(lexer->tokens);
  if (lexer->cursor < tokens_meta->count && tokens_meta->count > 0) {
    *out = lexer->tokens[lexer->cursor];
    return true;
  }
  return false;
}

void cxl_advance(struct CX_Lexer *lexer, size_t by) {
  struct CX_DArrayMeta *tokens_meta = cx_da_meta(lexer->tokens);
  if (lexer->cursor + by > tokens_meta->count) {
    lexer->cursor = tokens_meta->count;
  } else {
    lexer->cursor += by;
  }
}

bool cxl_token_consume(struct CX_Lexer *lexer, struct CX_Token *out) {
  struct CX_DArrayMeta *tokens_meta = cx_da_meta(lexer->tokens);
  if (lexer->cursor < tokens_meta->count) {
    *out = lexer->tokens[lexer->cursor++];
    return true;
  }
  return false;
}

CX_Location cxl_get_token_location(struct CX_Lexer *lexer, CX_Token tok) {
  CX_Location loc = {1, 1};

  const char *begin = lexer->file.contents.data;
  const char *ptr = begin;
  const char *end = tok.position.data;

  while (ptr < end) {
    if (*ptr == '\n') {
      loc.line_no++;
      loc.column = 1;
    } else {
      loc.column++;
    }

    ptr++;
  }

  return loc;
}

int _count_digits_base10(u64 x) {
  int n_digits = 1;
  while (x /= 10) n_digits++;
  return n_digits;
}
void cxl_print_token_location(struct CX_Lexer *lexer, CX_Token token) {
  char const *line_start = token.position.data;
  char const *line_end = token.position.data + token.position.count;

  char const *file_start = lexer->file.contents.data;
  char const *file_end = file_start + lexer->file.contents.count;
  while (line_start > file_start && *(line_start - 1) != '\n') {
    line_start--;
  }

  if (*line_end != '\n') { // line_end is \n if the token is last on the line
    while (line_end < file_end && *(line_end + 1) != '\n') {
      line_end++;
    }
  }
  size_t line_len = (size_t)(line_end - line_start);
  assert(line_len <= SSIZE_MAX);
  Nob_String_View line = nob_sv_from_parts(line_start, line_len + 1);
  line = nob_sv_trim_right(line);

  CX_Location loc = cxl_get_token_location(lexer, token);
  nob_log(NOB_INFO, " %zu |" SV_Fmt "", loc.line_no, SV_Arg(line));

  size_t tok_offset = (size_t)(token.position.data - line_start);
  assert(tok_offset <= SSIZE_MAX);
  int line_no_specifier_len = _count_digits_base10(loc.line_no);

  nob_log(NOB_INFO, " %*s |%*s^", line_no_specifier_len, " ", (int)tok_offset, " ");
}
