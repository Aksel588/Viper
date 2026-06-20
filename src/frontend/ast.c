#include "ast.h"

#include <string.h>

static AstNode *alloc_node(Arena *arena, AstKind kind, int line, int column) {
    AstNode *node = (AstNode *)arena_alloc(arena, sizeof(AstNode));
    if (!node) {
        return NULL;
    }
    memset(node, 0, sizeof(*node));
    node->kind = kind;
    node->line = line;
    node->column = column;
    node->inferred_type = viper_type_unknown();
    return node;
}

AstNode *ast_program(Arena *arena, const char *module_name, AstNode **statements, int count) {
    AstNode *node = alloc_node(arena, AST_PROGRAM, 1, 1);
    if (!node) {
        return NULL;
    }
    node->as.program.module_name = module_name ? arena_strdup(arena, module_name) : NULL;
    node->as.program.statements = (AstNode **)arena_alloc(arena, (size_t)count * sizeof(AstNode *));
    node->as.program.statement_count = count;
    for (int i = 0; i < count; i++) {
        node->as.program.statements[i] = statements[i];
    }
    return node;
}

AstNode *ast_var_decl(Arena *arena, int line, int col, const char *name, ViperType type, AstNode *value) {
    AstNode *node = alloc_node(arena, AST_VAR_DECL, line, col);
    if (!node) {
        return NULL;
    }
    node->as.var_decl.name = arena_strdup(arena, name);
    node->as.var_decl.type = type;
    node->as.var_decl.value = value;
    return node;
}

AstNode *ast_assign(Arena *arena, int line, int col, const char *name, AstNode *value) {
    AstNode *node = alloc_node(arena, AST_ASSIGN, line, col);
    if (!node) {
        return NULL;
    }
    node->as.assign.name = arena_strdup(arena, name);
    node->as.assign.value = value;
    return node;
}

AstNode *ast_binary(Arena *arena, int line, int col, BinOp op, AstNode *left, AstNode *right) {
    AstNode *node = alloc_node(arena, AST_BINARY, line, col);
    if (!node) {
        return NULL;
    }
    node->as.binary.op = op;
    node->as.binary.left = left;
    node->as.binary.right = right;
    return node;
}

AstNode *ast_unary(Arena *arena, int line, int col, BinOp op, AstNode *operand) {
    AstNode *node = alloc_node(arena, AST_UNARY, line, col);
    if (!node) {
        return NULL;
    }
    node->as.unary.op = op;
    node->as.unary.operand = operand;
    return node;
}

AstNode *ast_literal_int(Arena *arena, int line, int col, long long value) {
    AstNode *node = alloc_node(arena, AST_LITERAL_INT, line, col);
    if (!node) {
        return NULL;
    }
    node->as.literal_int.value = value;
    node->inferred_type = viper_type_int();
    return node;
}

AstNode *ast_literal_float(Arena *arena, int line, int col, double value) {
    AstNode *node = alloc_node(arena, AST_LITERAL_FLOAT, line, col);
    if (!node) {
        return NULL;
    }
    node->as.literal_float.value = value;
    node->inferred_type = viper_type_float();
    return node;
}

AstNode *ast_literal_string(Arena *arena, int line, int col, const char *value) {
    AstNode *node = alloc_node(arena, AST_LITERAL_STRING, line, col);
    if (!node) {
        return NULL;
    }
    node->as.literal_string.value = arena_strdup(arena, value);
    node->inferred_type = viper_type_string();
    return node;
}

AstNode *ast_literal_bool(Arena *arena, int line, int col, bool value) {
    AstNode *node = alloc_node(arena, AST_LITERAL_BOOL, line, col);
    if (!node) {
        return NULL;
    }
    node->as.literal_bool.value = value;
    node->inferred_type = viper_type_bool();
    return node;
}

AstNode *ast_literal_tensor(Arena *arena, int line, int col, AstNode **rows, int row_count) {
    AstNode *node = alloc_node(arena, AST_LITERAL_TENSOR, line, col);
    if (!node) {
        return NULL;
    }
    node->as.literal_tensor.rows = (AstNode **)arena_alloc(arena, (size_t)row_count * sizeof(AstNode *));
    node->as.literal_tensor.row_count = row_count;
    for (int i = 0; i < row_count; i++) {
        node->as.literal_tensor.rows[i] = rows[i];
    }
    return node;
}

AstNode *ast_identifier(Arena *arena, int line, int col, const char *name) {
    AstNode *node = alloc_node(arena, AST_IDENTIFIER, line, col);
    if (!node) {
        return NULL;
    }
    node->as.identifier.name = arena_strdup(arena, name);
    return node;
}

AstNode *ast_if(Arena *arena, int line, int col, AstNode *cond, AstNode *then_b, AstNode *else_b) {
    AstNode *node = alloc_node(arena, AST_IF, line, col);
    if (!node) {
        return NULL;
    }
    node->as.if_stmt.condition = cond;
    node->as.if_stmt.then_branch = then_b;
    node->as.if_stmt.else_branch = else_b;
    return node;
}

AstNode *ast_for(Arena *arena, int line, int col, const char *name, AstNode *start, AstNode *end, AstNode *body) {
    AstNode *node = alloc_node(arena, AST_FOR, line, col);
    if (!node) {
        return NULL;
    }
    node->as.for_stmt.name = arena_strdup(arena, name);
    node->as.for_stmt.start = start;
    node->as.for_stmt.end = end;
    node->as.for_stmt.body = body;
    return node;
}

AstNode *ast_block(Arena *arena, int line, int col, AstNode **statements, int count) {
    AstNode *node = alloc_node(arena, AST_BLOCK, line, col);
    if (!node) {
        return NULL;
    }
    node->as.block.statements = (AstNode **)arena_alloc(arena, (size_t)count * sizeof(AstNode *));
    node->as.block.statement_count = count;
    for (int i = 0; i < count; i++) {
        node->as.block.statements[i] = statements[i];
    }
    return node;
}

AstNode *ast_fn_decl(Arena *arena, int line, int col, const char *name, AstParam *params, int param_count,
                     ViperType return_type, AstNode *body, bool exported, const char *module_name) {
    AstNode *node = alloc_node(arena, AST_FN_DECL, line, col);
    if (!node) {
        return NULL;
    }
    node->as.fn_decl.name = arena_strdup(arena, name);
    node->as.fn_decl.params = params;
    node->as.fn_decl.param_count = param_count;
    node->as.fn_decl.return_type = return_type;
    node->as.fn_decl.body = body;
    node->as.fn_decl.exported = exported;
    node->as.fn_decl.module_name = module_name ? arena_strdup(arena, module_name) : NULL;
    return node;
}

AstNode *ast_call(Arena *arena, int line, int col, const char *module, const char *name, AstNode **args,
                  int arg_count) {
    AstNode *node = alloc_node(arena, AST_CALL, line, col);
    if (!node) {
        return NULL;
    }
    node->as.call.module = module ? arena_strdup(arena, module) : NULL;
    node->as.call.name = arena_strdup(arena, name);
    node->as.call.args = (AstNode **)arena_alloc(arena, (size_t)arg_count * sizeof(AstNode *));
    node->as.call.arg_count = arg_count;
    for (int i = 0; i < arg_count; i++) {
        node->as.call.args[i] = args[i];
    }
    return node;
}

AstNode *ast_return(Arena *arena, int line, int col, AstNode *value) {
    AstNode *node = alloc_node(arena, AST_RETURN, line, col);
    if (!node) {
        return NULL;
    }
    node->as.return_stmt.value = value;
    return node;
}

AstNode *ast_expr_stmt(Arena *arena, int line, int col, AstNode *expr) {
    AstNode *node = alloc_node(arena, AST_EXPR_STMT, line, col);
    if (!node) {
        return NULL;
    }
    node->as.expr_stmt.expr = expr;
    return node;
}

AstNode *ast_print(Arena *arena, int line, int col, AstNode *expr) {
    AstNode *node = alloc_node(arena, AST_PRINT, line, col);
    if (!node) {
        return NULL;
    }
    node->as.print_stmt.expr = expr;
    return node;
}

AstNode *ast_cast(Arena *arena, int line, int col, ViperType target, AstNode *operand) {
    AstNode *node = alloc_node(arena, AST_CAST, line, col);
    if (!node) {
        return NULL;
    }
    node->as.cast.target = target;
    node->as.cast.operand = operand;
    return node;
}

AstNode *ast_index(Arena *arena, int line, int col, AstNode *base, AstNode **indices, int index_count) {
    AstNode *node = alloc_node(arena, AST_INDEX, line, col);
    if (!node) {
        return NULL;
    }
    node->as.index.base = base;
    node->as.index.indices = (AstNode **)arena_alloc(arena, (size_t)index_count * sizeof(AstNode *));
    node->as.index.index_count = index_count;
    for (int i = 0; i < index_count; i++) {
        node->as.index.indices[i] = indices[i];
    }
    return node;
}

AstNode *ast_module_decl(Arena *arena, int line, int col, const char *name) {
    AstNode *node = alloc_node(arena, AST_MODULE_DECL, line, col);
    if (!node) {
        return NULL;
    }
    node->as.module_decl.name = arena_strdup(arena, name);
    return node;
}

AstNode *ast_open(Arena *arena, int line, int col, const char *module_name) {
    AstNode *node = alloc_node(arena, AST_OPEN_STMT, line, col);
    if (!node) {
        return NULL;
    }
    node->as.open_stmt.module_name = arena_strdup(arena, module_name);
    return node;
}

AstNode *ast_while(Arena *arena, int line, int col, AstNode *cond, AstNode *body) {
    AstNode *node = alloc_node(arena, AST_WHILE, line, col);
    if (!node) {
        return NULL;
    }
    node->as.while_stmt.condition = cond;
    node->as.while_stmt.body = body;
    return node;
}

AstNode *ast_qualified_name(Arena *arena, int line, int col, const char *module, const char *name) {
    AstNode *node = alloc_node(arena, AST_QUALIFIED_NAME, line, col);
    if (!node) {
        return NULL;
    }
    node->as.qualified_name.module = arena_strdup(arena, module);
    node->as.qualified_name.name = arena_strdup(arena, name);
    return node;
}
