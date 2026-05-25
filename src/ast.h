#pragma once
#include "common.h"
#include "lexer.h"

typedef enum CX_OperatorKind {
  CX_OPERATOR_NULL,
  CX_OPERATOR_BINARY,
  CX_OPERATOR_UNARY_POSTFIX,
  CX_OPERATOR_UNARY_PREFIX,
} CX_OperatorKind;

// @note this MUST match 1:1 to token kind
typedef enum CX_Operator {
  CX_OP_ADD = CX_TOKEN_ADD,
  CX_OP_SUB = CX_TOKEN_SUB,
  CX_OP_MUL = CX_TOKEN_MUL,
  CX_OP_DIV = CX_TOKEN_DIV,
  CX_OP_EQ = CX_TOKEN_EQ,
  CX_OP_REF = CX_TOKEN_REF,
  CX_OP_DEREF = CX_TOKEN_DEREF,
  CX_OP_DOT = CX_TOKEN_DOT,
} CX_Operator;

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
  CX_EXPR_UOP,

  CX_EXPR_PROCDECL,
  CX_EXPR_IF,
  CX_EXPR_WHILE,
  CX_EXPR_PROC,
  CX_EXPR_BLOCK,
};

typedef struct CX_Type {
  Nob_String_View inner;
  size_t ptr_depth;
} CX_Type;

typedef struct CX_Expression_ProcCall CX_Expression_ProcCall;
typedef struct CX_Expression CX_Expression;

typedef CX_Array(CX_Expression *) CX_Tuple;

struct CX_Expression_ProcCall {
  Nob_String_View proc_name;
  CX_Tuple params;
};

typedef struct CX_Expression_IfCondition {
  CX_Expression *condition;
  CX_Expression *then;
  CX_Expression *else_;
} CX_Expression_IfCondition;

typedef struct CX_Expression_While {
  CX_Expression *condition;
  CX_Expression *then;
} CX_Expression_While;

typedef struct CX_Statement_VarDecl {
  Nob_String_View name;
  struct CX_Type type;
  struct CX_Expression *value;
} CX_Statement_VarDecl;

enum CX_StatementKind {
  CX_STMT_VARDECL,
  CX_STMT_EXPR,
};

typedef struct CX_Statement {
  enum CX_StatementKind kind;
  union {
    struct CX_Statement_VarDecl var_decl;
    struct CX_Expression *expr;
  } _As;
} CX_Statement;

typedef struct CX_Block {
  struct CX_Statement_VarDecl decl;
  CX_Array(CX_Statement) stmts;
} CX_Block;

typedef struct CX_BinaryOperation {
  CX_Operator op;
  CX_Expression *left;
  CX_Expression *right;
} CX_BinaryOperation;

typedef struct CX_UnaryOperation {
  CX_Operator op;
  CX_Expression *expr;
} CX_UnaryOperation;

struct CX_Expression {
  enum CX_ExpressionKind kind;
  union {
    CX_Number number;
    Nob_String_View literal;
    Nob_String_View var_name;
    CX_Expression_ProcCall call;
    CX_BinaryOperation binop;
    CX_UnaryOperation uop;
    CX_Expression_IfCondition if_;
    CX_Expression_While while_;
    CX_Block block;
  } _As;
};

typedef struct CX_VarAndType {
  Nob_String_View name;
  struct CX_Type type;
} CX_VarAndType;

typedef CX_VarAndType CX_Argument;
typedef CX_VarAndType CX_PlexField;

typedef struct CX_Plex {
  Nob_String_View name;
  CX_Array(CX_PlexField) fields;

} CX_Plex;
typedef struct CX_ProcedureDecl {
  Nob_String_View name;
  CX_Array(CX_Argument) args;
  struct CX_Type return_type;
} CX_ProcedureDecl;

typedef struct CX_Procedure {
  struct CX_ProcedureDecl decl;
  struct CX_Block block;
} CX_Procedure;

typedef struct CX_Module {
  // @todo these should be arena arrays
  CX_Array(CX_ProcedureDecl) proc_decls;
  CX_Array(CX_Procedure) procs;
} CX_Module;
