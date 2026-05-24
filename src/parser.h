#pragma once

#include "ast.h"
#include "common.h"
#include "lexer.h"
#include <arena.h>
#include <sys/types.h>
typedef struct CX_Parser {
  CX_Array(struct CX_ProcedureDecl) proc_decls;
  CX_Array(struct CX_Procedure) procs;
  CX_Lexer lexer;
  Arena storage_arena;
} CX_Parser;
