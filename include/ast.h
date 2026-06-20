#ifndef VIPER_AST_H
#define VIPER_AST_H

#include "arena.h"
#include "types.h"

typedef enum {
    AST_PROGRAM,
    AST_VAR_DECL,
    AST_ASSIGN,
    AST_BINARY,
    AST_UNARY,
    AST_LITERAL_INT,
    AST_LITERAL_FLOAT,
    AST_LITERAL_STRING,
    AST_LITERAL_BOOL,
    AST_LITERAL_TENSOR,
    AST_IDENTIFIER,
    AST_IF,
    AST_FOR,
    AST_BLOCK,
    AST_FN_DECL,
    AST_CALL,
    AST_RETURN,
    AST_EXPR_STMT,
    AST_PRINT,
    AST_CAST,
    AST_INDEX,
    AST_MODULE_DECL,
    AST_OPEN_STMT,
    AST_WHILE,
    AST_QUALIFIED_NAME
} AstKind;

typedef enum {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MATMUL,
    OP_EQ,
    OP_NEQ,
    OP_LT,
    OP_GT,
    OP_LTE,
    OP_GTE
} BinOp;

typedef struct AstNode AstNode;

typedef struct AstParam {
    char *name;
    ViperType type;
} AstParam;

typedef struct AstNode {
    AstKind kind;
    int line;
    int column;
    ViperType inferred_type;
    AstNode *next;
    union {
        struct {
            AstNode **statements;
            int statement_count;
            char *module_name;
        } program;
        struct {
            char *name;
            ViperType type;
            AstNode *value;
        } var_decl;
        struct {
            char *name;
            AstNode *value;
        } assign;
        struct {
            BinOp op;
            AstNode *left;
            AstNode *right;
        } binary;
        struct {
            BinOp op;
            AstNode *operand;
        } unary;
        struct {
            long long value;
        } literal_int;
        struct {
            double value;
        } literal_float;
        struct {
            char *value;
        } literal_string;
        struct {
            bool value;
        } literal_bool;
        struct {
            AstNode **rows;
            int row_count;
        } literal_tensor;
        struct {
            char *name;
        } identifier;
        struct {
            AstNode *condition;
            AstNode *then_branch;
            AstNode *else_branch;
        } if_stmt;
        struct {
            char *name;
            AstNode *start;
            AstNode *end;
            AstNode *body;
        } for_stmt;
        struct {
            AstNode **statements;
            int statement_count;
        } block;
        struct {
            char *name;
            AstParam *params;
            int param_count;
            ViperType return_type;
            AstNode *body;
            bool exported;
            char *module_name;
        } fn_decl;
        struct {
            char *name;
            AstNode **args;
            int arg_count;
            char *module;
        } call;
        struct {
            AstNode *value;
        } return_stmt;
        struct {
            AstNode *expr;
        } expr_stmt;
        struct {
            AstNode *expr;
        } print_stmt;
        struct {
            ViperType target;
            AstNode *operand;
        } cast;
        struct {
            AstNode *base;
            AstNode **indices;
            int index_count;
        } index;
        struct {
            char *name;
        } module_decl;
        struct {
            char *module_name;
        } open_stmt;
        struct {
            AstNode *condition;
            AstNode *body;
        } while_stmt;
        struct {
            char *module;
            char *name;
        } qualified_name;
    } as;
} AstNode;

AstNode *ast_program(Arena *arena, const char *module_name, AstNode **statements, int count);
AstNode *ast_var_decl(Arena *arena, int line, int col, const char *name, ViperType type, AstNode *value);
AstNode *ast_assign(Arena *arena, int line, int col, const char *name, AstNode *value);
AstNode *ast_binary(Arena *arena, int line, int col, BinOp op, AstNode *left, AstNode *right);
AstNode *ast_unary(Arena *arena, int line, int col, BinOp op, AstNode *operand);
AstNode *ast_literal_int(Arena *arena, int line, int col, long long value);
AstNode *ast_literal_float(Arena *arena, int line, int col, double value);
AstNode *ast_literal_string(Arena *arena, int line, int col, const char *value);
AstNode *ast_literal_bool(Arena *arena, int line, int col, bool value);
AstNode *ast_literal_tensor(Arena *arena, int line, int col, AstNode **rows, int row_count);
AstNode *ast_identifier(Arena *arena, int line, int col, const char *name);
AstNode *ast_if(Arena *arena, int line, int col, AstNode *cond, AstNode *then_b, AstNode *else_b);
AstNode *ast_for(Arena *arena, int line, int col, const char *name, AstNode *start, AstNode *end, AstNode *body);
AstNode *ast_block(Arena *arena, int line, int col, AstNode **statements, int count);
AstNode *ast_fn_decl(Arena *arena, int line, int col, const char *name, AstParam *params, int param_count,
                     ViperType return_type, AstNode *body, bool exported, const char *module_name);
AstNode *ast_call(Arena *arena, int line, int col, const char *module, const char *name, AstNode **args,
                  int arg_count);
AstNode *ast_return(Arena *arena, int line, int col, AstNode *value);
AstNode *ast_expr_stmt(Arena *arena, int line, int col, AstNode *expr);
AstNode *ast_print(Arena *arena, int line, int col, AstNode *expr);
AstNode *ast_cast(Arena *arena, int line, int col, ViperType target, AstNode *operand);
AstNode *ast_index(Arena *arena, int line, int col, AstNode *base, AstNode **indices, int index_count);
AstNode *ast_module_decl(Arena *arena, int line, int col, const char *name);
AstNode *ast_open(Arena *arena, int line, int col, const char *module_name);
AstNode *ast_while(Arena *arena, int line, int col, AstNode *cond, AstNode *body);
AstNode *ast_qualified_name(Arena *arena, int line, int col, const char *module, const char *name);

#endif
