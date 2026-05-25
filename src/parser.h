#pragma once

#include "ast.h"
#include "lexer.h"
#include <arena.h>
#include <sys/types.h>
typedef struct CX_Parser {
  CX_Lexer lexer;
  Arena storage_arena;
  CX_Module mod;
} CX_Parser;
