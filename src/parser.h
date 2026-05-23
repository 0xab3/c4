#pragma once

#include "ast.h"
#include "common.h"
struct CX_Parser {
  CX_Array(struct CX_ProcedureDecl) proc_decls;
  CX_Array(struct CX_Procedure) procs;
};
