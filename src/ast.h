#pragma once
#include "common.h"
#include "lexer.h"

typedef enum CX_ParseError {
  CXPE_OK,
  CXPE_UNEXPECTED_TOKEN,
} CX_ParseError;

enum CX_ExpressionKind {
  CX_EXPR_NUMBER,
  CX_EXPR_LITERAL,
  CX_EXPR_CALL,
  CX_EXPR_VAR,
  CX_EXPR_BINOP,

  CX_EXPR_PROCDECL,
  CX_EXPR_PROC,
  CX_EXPR_BLOCK,
};

typedef struct CX_Type {
  Nob_String_View inner;
  size_t ptr_depth;
} CX_Type;

typedef struct CX_Expression_ProcCall CX_Expression_ProcCall;
typedef struct CX_Expression CX_Expression;

struct CX_Expression_ProcCall {
  Nob_String_View proc_name;
  CX_Array(CX_Expression *) params;
};

typedef struct CX_BinaryOperation {
  enum CX_Operand op;
  struct CX_Expression *left;
  struct CX_Expression *right;
} CX_BinaryOperation;

struct CX_Expression {
  enum CX_ExpressionKind kind;
  union {
    CX_Number number;
    Nob_String_View literal;
    Nob_String_View var_name;
    CX_Expression_ProcCall call;
    CX_BinaryOperation binop;
  } _As;
};

enum CX_StatementKind {
  CX_STMT_VARDECL,
  CX_STMT_EXPR,
  CX_STMT_IF,
};

typedef struct CX_Statement_VarDecl {
  Nob_String_View name;
  struct CX_Type type;
  struct CX_Expression *value;
} CX_Statement_VarDecl;

typedef struct CX_Statement {
  enum CX_StatementKind kind;
  union {
    struct CX_Statement_VarDecl var_decl;
    struct CX_Expression *expr;
  } _As;
} CX_Statement;

struct CX_Block {
  struct CX_Statement_VarDecl decl;
  CX_Array(struct CX_Statement) stmts;
};

typedef struct CX_Argument {
  Nob_String_View name;
  struct CX_Type type;
} CX_Argument;

struct CX_ProcedureDecl {
  Nob_String_View name;
  CX_Array(CX_Argument) args;
  struct CX_Type return_type;
  bool has_body;
};

struct CX_Procedure {
  struct CX_ProcedureDecl decl;
  struct CX_Block block;
};
