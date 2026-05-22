#pragma once
#include <stddef.h>
#include "../nob.h"
#include <stdint.h>
typedef int64_t C4_Number;

enum C4_TokenKind {
  C4_TOKEN_CURLY_OPEN = '{',
  C4_TOKEN_CURLY_CLOSE = '}',
  C4_TOKEN_PAREN_OPEN = '(',
  C4_TOKEN_PAREN_CLOSE = ')',
  C4_TOKEN_SEMI = ';',
  C4_TOKEN_COLON = ':',
  C4_TOKEN_COMMA = ',',
  C4_TOKEN_DOT = '.',
  C4_TOKEN_NOT = '!',
  C4_TOKEN_POINTER = '^',
  C4_TOKEN_ASS = '=',
  C4_TOKEN_ADD = '+',
  C4_TOKEN_SUB = '-',
  C4_TOKEN_MUL = '*',
  C4_TOKEN_DIV = '/',

  C4_TOKEN_LT = '<',
  C4_TOKEN_GT = '>',

  // multi character symbols
  C4_TOKEN_ARROW = 128,

  C4_TOKEN_SUBASS,
  C4_TOKEN_ADDASS,
  C4_TOKEN_MULASS,
  C4_TOKEN_DIVASS,

  C4_TOKEN_EQ,
  C4_TOKEN_NEQ,
  C4_TOKEN_LTEQ,
  C4_TOKEN_GTEQ,

  // keyword
  C4_TOKEN_PROCDECL,
  C4_TOKEN_VARDEF,
  C4_TOKEN_RETURN,
  C4_TOKEN_IF,
  C4_TOKEN_ELSE,
  C4_TOKEN_WHILE,
  C4_TOKEN_PLEX,

  C4_TOKEN_STRING_LITERAL,
  C4_TOKEN_NUMBER,

  C4_TOKEN_IDENTIFIER,
};
enum C4_Operand {
  C4_OP_ADD = C4_TOKEN_ADD,
  C4_OP_SUB = C4_TOKEN_SUB,
  C4_OP_MUL = C4_TOKEN_MUL,
  C4_OP_DIV = C4_TOKEN_DIV,
};

struct C4_Token {
  enum C4_TokenKind kind;
  union {
    Nob_String_View literal;
    C4_Number number;
    Nob_String_View identifier;
    uint8_t none;
  } _As;

  Nob_String_View position;
};
struct C4_TokenName {
  char const *name;
  enum C4_TokenKind kind;
};
struct C4_TokenName C4LEX_KEYWORDS[] = {
    {"proc", C4_TOKEN_PROCDECL}, {"let", C4_TOKEN_VARDEF},
    {"return", C4_TOKEN_RETURN}, {"if", C4_TOKEN_IF},
    {"else", C4_TOKEN_ELSE},     {"while", C4_TOKEN_WHILE},
    {"plex", C4_TOKEN_PLEX},
};

struct C4_TokenName C4LEX_MULTI_CHARACTER_SYMBOLS[] = {
    {"->", C4_TOKEN_ARROW},  {"-=", C4_TOKEN_SUBASS}, {"+=", C4_TOKEN_ADDASS},
    {"*=", C4_TOKEN_MULASS}, {"/=", C4_TOKEN_DIVASS}, {"==", C4_TOKEN_EQ},
    {"!=", C4_TOKEN_NEQ},    {"<=", C4_TOKEN_LTEQ},   {">=", C4_TOKEN_GTEQ},
};
struct C4_Tokens {
  struct C4_Token *items;
  size_t count;
  size_t capacity;
};

struct C4_Lexer {};

#define C4LEX_SV_NULL                                                          \
  (Nob_String_View) {}

#define C4LEX_TOKEN_MAKE(_kind, _field, value, _position)                      \
  (struct C4_Token){.kind = _kind, ._As._field = value, .position = _position};
