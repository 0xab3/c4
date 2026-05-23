#pragma once
#include "common.h"
#include "lexer.h"

enum CX_ParseError {
  CXPE_OK,
  CXPE_UNEXPECTED_TOKEN,
};
enum CX_ExpressionKind {
  CX_EXPR_NUMBER,
  CX_EXPR_LITERAL,
  CX_EXPR_CALL,
  CX_EXPR_BINOP,

  CX_EXPR_PROCDECL,
  CX_EXPR_PROC,
};

struct CX_Type {
  Nob_String_View inner;
  size_t ptr_depth;
};

struct CX_Expression {
  enum CX_ExpressionKind kind;
  union {
    CX_Number number;
    Nob_String_View literal;
  } _As;
};

enum CX_StatementKind {
  CX_STMT_VARDECL,
  CX_STMT_EXPR,
};

struct CX_Statement_VarDecl {
  Nob_String_View name;
  struct CX_Type type;
  struct CX_Expression *value;
};

struct CX_Statement {
  enum CX_StatementKind kind;
  union {
    struct CX_Statement_VarDecl var_decl;
    struct CX_Expression *expr;
  } _As;
};

struct CX_Statements {
  struct CX_Statement *items;
  size_t count;
  size_t capacity;
};

struct CX_Block {
  struct CX_Statement_VarDecl decl;
  CX_Array(struct CX_Statements) stmts;
};
struct CX_BinaryOperation {
  enum CX_Operand op;
  struct CX_Expression *left;
  struct CX_Expression *right;
};

struct CX_Argument {
  Nob_String_View name;
  struct CX_Type type;
};
struct CX_ProcedureDecl {
  Nob_String_View name;
  CX_Array(struct CX_Argument) args;
  struct CX_Type return_type;
  bool has_body;
};

struct CX_Procedure {
  struct CX_ProcedureDecl decl;
  struct CX_Block block;
};
