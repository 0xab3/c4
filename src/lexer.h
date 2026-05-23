#pragma once
#include "../nob.h"
#include "common.h"
#include <stddef.h>
#include <stdint.h>
typedef int64_t CX_Number;

enum CX_TokenKind {
  CX_TOKEN_CURLY_OPEN = '{',
  CX_TOKEN_CURLY_CLOSE = '}',
  CX_TOKEN_PAREN_OPEN = '(',
  CX_TOKEN_PAREN_CLOSE = ')',
  CX_TOKEN_SEMI = ';',
  CX_TOKEN_COLON = ':',
  CX_TOKEN_COMMA = ',',
  CX_TOKEN_DOT = '.',
  CX_TOKEN_NOT = '!',
  CX_TOKEN_POINTER = '^',
  CX_TOKEN_ASS = '=',
  CX_TOKEN_ADD = '+',
  CX_TOKEN_SUB = '-',
  CX_TOKEN_MUL = '*',
  CX_TOKEN_DIV = '/',

  CX_TOKEN_LT = '<',
  CX_TOKEN_GT = '>',

  // multi character symbols
  CX_TOKEN_ARROW = 128,

  CX_TOKEN_SUBASS,
  CX_TOKEN_ADDASS,
  CX_TOKEN_MULASS,
  CX_TOKEN_DIVASS,

  CX_TOKEN_EQ,
  CX_TOKEN_NEQ,
  CX_TOKEN_LTEQ,
  CX_TOKEN_GTEQ,

  // keyword
  CX_TOKEN_PROCDECL,
  CX_TOKEN_VARDEF,
  CX_TOKEN_RETURN,
  CX_TOKEN_IF,
  CX_TOKEN_ELSE,
  CX_TOKEN_WHILE,
  CX_TOKEN_PLEX,

  CX_TOKEN_STRING_LITERAL,
  CX_TOKEN_NUMBER,

  CX_TOKEN_IDENTIFIER,

  CX_TOKEN_EOF,
};
enum CX_Operand {
  CX_OP_ADD = CX_TOKEN_ADD,
  CX_OP_SUB = CX_TOKEN_SUB,
  CX_OP_MUL = CX_TOKEN_MUL,
  CX_OP_DIV = CX_TOKEN_DIV,
};

struct CX_Token {
  enum CX_TokenKind kind;
  union {
    Nob_String_View literal;
    CX_Number number;
    Nob_String_View identifier;
    uint8_t none;
  } _As;

  Nob_String_View position;
};
struct CX_TokenName {
  char const *name;
  enum CX_TokenKind kind;
};
struct CX_TokenName CXLEX_KEYWORDS[] = {
    {"proc", CX_TOKEN_PROCDECL}, {"let", CX_TOKEN_VARDEF},
    {"return", CX_TOKEN_RETURN}, {"if", CX_TOKEN_IF},
    {"else", CX_TOKEN_ELSE},     {"while", CX_TOKEN_WHILE},
    {"plex", CX_TOKEN_PLEX},
};

struct CX_TokenName CXLEX_MULTI_CHARACTER_SYMBOLS[] = {
    {"->", CX_TOKEN_ARROW},  {"-=", CX_TOKEN_SUBASS}, {"+=", CX_TOKEN_ADDASS},
    {"*=", CX_TOKEN_MULASS}, {"/=", CX_TOKEN_DIVASS}, {"==", CX_TOKEN_EQ},
    {"!=", CX_TOKEN_NEQ},    {"<=", CX_TOKEN_LTEQ},   {">=", CX_TOKEN_GTEQ},
};

struct CX_Lexer {
  CX_Array(struct CX_Token) tokens;
  size_t cursor;
};

#define CXLEX_SV_NULL                                                          \
  (Nob_String_View) {}

#define CXLEX_TOKEN_MAKE(_kind, _field, value, _position)                      \
  (struct CX_Token){.kind = _kind, ._As._field = value, .position = _position};

bool cxl_token_consume(struct CX_Lexer *lexer, struct CX_Token *out);
void cxl_token_advance(struct CX_Lexer *lexer, size_t by);
struct CX_Token cxl_token_peek(struct CX_Lexer *lexer);
bool cxl_token_try_peek(struct CX_Lexer *lexer, struct CX_Token *out);
char const *cxl_token_kind_to_string(enum CX_TokenKind kind);
