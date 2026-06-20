#include "semantic.h"

#include "builtins.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool check_expr(SemanticContext *ctx, AstNode *node);
static bool check_stmt(SemanticContext *ctx, AstNode *node);

static void type_error(SemanticContext *ctx, AstNode *node, const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    diag_emit(ctx->diag, DIAG_ERROR, ctx->file, node->line, node->column, "%s", buf);
}

static char *sem_strdup(SemanticContext *ctx, const char *s) {
    if (!ctx->arena || !s) {
        return NULL;
    }
    return arena_strdup(ctx->arena, s);
}

static void symbol_free_local(Symbol *sym) {
    free(sym->name);
    free(sym->simple_name);
    free(sym->module_name);
    free(sym->param_types);
    free(sym->param_names);
    free(sym);
}

static Symbol *lookup_function(SemanticContext *ctx, const char *module, const char *name, AstNode *call_node) {
    if (module) {
        return symtab_lookup_qualified(ctx->symtab, module, name);
    }

    Symbol *local = symtab_lookup(ctx->symtab, name);
    if (local && local->kind == SYM_FN) {
        return local;
    }

    Symbol *opened = symtab_lookup_with_opens(ctx->symtab, name, (const char **)ctx->opened_modules,
                                              ctx->open_count);
    if (opened && call_node && call_node->kind == AST_CALL && !call_node->as.call.module) {
        call_node->as.call.module = sem_strdup(ctx, opened->module_name);
    }
    if (opened) {
        return opened;
    }

    return symtab_lookup_global(ctx->symtab, name);
}

static bool register_file_function(SemanticContext *ctx, AstNode *fn_node) {
    Symbol *sym = symbol_create(fn_node->as.fn_decl.name, SYM_FN, fn_node->as.fn_decl.return_type, fn_node->line,
                                fn_node->column);
    if (!sym) {
        return false;
    }
    sym->module_name = fn_node->as.fn_decl.module_name ? strdup(fn_node->as.fn_decl.module_name) : NULL;
    sym->exported = fn_node->as.fn_decl.exported;
    sym->param_count = fn_node->as.fn_decl.param_count;
    if (sym->param_count > 0) {
        sym->param_types = (ViperType *)calloc((size_t)sym->param_count, sizeof(ViperType));
        sym->param_names = (char **)calloc((size_t)sym->param_count, sizeof(char *));
        for (int i = 0; i < sym->param_count; i++) {
            sym->param_types[i] = fn_node->as.fn_decl.params[i].type;
            sym->param_names[i] = strdup(fn_node->as.fn_decl.params[i].name);
        }
    }
    if (!symtab_insert(ctx->symtab, sym)) {
        type_error(ctx, fn_node, "duplicate function '%s' in module", fn_node->as.fn_decl.name);
        symbol_free_local(sym);
        return false;
    }
    return true;
}

static bool infer_tensor_literal(SemanticContext *ctx, AstNode *node, ViperType *out) {
    if (node->kind != AST_LITERAL_TENSOR) {
        return false;
    }
    int row_count = node->as.literal_tensor.row_count;
    if (row_count == 0) {
        *out = viper_type_tensor(TYPE_FLOAT, (int[]){0, 0}, 2);
        return true;
    }

    AstNode *first_row = node->as.literal_tensor.rows[0];
    if (first_row->kind != AST_BLOCK) {
        type_error(ctx, node, "invalid tensor row");
        return false;
    }
    int cols = first_row->as.block.statement_count;
    TypeKind elem = TYPE_UNKNOWN;

    for (int r = 0; r < row_count; r++) {
        AstNode *row = node->as.literal_tensor.rows[r];
        if (row->kind != AST_BLOCK || row->as.block.statement_count != cols) {
            type_error(ctx, node, "tensor rows must have uniform length");
            return false;
        }
        for (int c = 0; c < cols; c++) {
            AstNode *elem_node = row->as.block.statements[c];
            if (!check_expr(ctx, elem_node)) {
                return false;
            }
            if (elem_node->inferred_type.kind != TYPE_INT &&
                elem_node->inferred_type.kind != TYPE_FLOAT) {
                type_error(ctx, elem_node, "tensor elements must be int or float");
                return false;
            }
            if (elem == TYPE_UNKNOWN) {
                elem = elem_node->inferred_type.kind;
            } else if (elem != elem_node->inferred_type.kind) {
                type_error(ctx, elem_node, "tensor elements must have uniform type");
                return false;
            }
        }
    }

    int dims[2] = {row_count, cols};
    *out = viper_type_tensor(elem, dims, 2);
    node->inferred_type = *out;
    return true;
}

static bool check_call(SemanticContext *ctx, AstNode *node) {
    if (builtins_is_transpose(node->as.call.name) && !node->as.call.module) {
        if (node->as.call.arg_count != 1) {
            type_error(ctx, node, "transpose expects 1 argument");
            return false;
        }
        if (!check_expr(ctx, node->as.call.args[0])) {
            return false;
        }
        ViperType out;
        if (!tensor_transpose_result(&node->as.call.args[0]->inferred_type, &out)) {
            type_error(ctx, node, "transpose requires a 2D tensor");
            return false;
        }
        node->inferred_type = out;
        return true;
    }
    if (builtins_is_reshape(node->as.call.name) && !node->as.call.module) {
        if (node->as.call.arg_count < 2) {
            type_error(ctx, node, "reshape expects tensor and dimension arguments");
            return false;
        }
        if (!check_expr(ctx, node->as.call.args[0])) {
            return false;
        }
        int rank = node->as.call.arg_count - 1;
        int dims[VIPER_TENSOR_MAX_RANK];
        for (int i = 0; i < rank; i++) {
            if (!check_expr(ctx, node->as.call.args[i + 1])) {
                return false;
            }
            if (node->as.call.args[i + 1]->inferred_type.kind != TYPE_INT) {
                type_error(ctx, node, "reshape dimensions must be int");
                return false;
            }
            if (node->as.call.args[i + 1]->kind != AST_LITERAL_INT) {
                type_error(ctx, node, "reshape dimensions must be integer literals");
                return false;
            }
            dims[i] = (int)node->as.call.args[i + 1]->as.literal_int.value;
        }
        ViperType out;
        if (!tensor_reshape_valid(&node->as.call.args[0]->inferred_type, dims, rank, &out)) {
            type_error(ctx, node, "invalid reshape: element count mismatch");
            return false;
        }
        node->inferred_type = out;
        return true;
    }

    for (int i = 0; i < node->as.call.arg_count; i++) {
        if (!check_expr(ctx, node->as.call.args[i])) {
            return false;
        }
    }

    Symbol *sym = lookup_function(ctx, node->as.call.module, node->as.call.name, node);
    if (!sym || sym->kind != SYM_FN) {
        if (node->as.call.module) {
            type_error(ctx, node, "undefined function '%s.%s'", node->as.call.module, node->as.call.name);
        } else {
            type_error(ctx, node, "undefined function '%s'", node->as.call.name);
        }
        return false;
    }
    if (node->as.call.arg_count != sym->param_count) {
        type_error(ctx, node, "function '%s' expects %d arguments, got %d", node->as.call.name, sym->param_count,
                   node->as.call.arg_count);
        return false;
    }
    for (int i = 0; i < node->as.call.arg_count; i++) {
        if (!viper_type_eq(&node->as.call.args[i]->inferred_type, &sym->param_types[i])) {
            char expected[64], got[64];
            viper_type_to_string(&sym->param_types[i], expected, sizeof(expected));
            viper_type_to_string(&node->as.call.args[i]->inferred_type, got, sizeof(got));
            type_error(ctx, node, "argument %d type mismatch: expected %s, got %s", i + 1, expected, got);
            return false;
        }
    }
    node->inferred_type = sym->type;
    return true;
}

static bool check_expr(SemanticContext *ctx, AstNode *node) {
    if (!node) {
        return false;
    }

    switch (node->kind) {
    case AST_LITERAL_INT:
        node->inferred_type = viper_type_int();
        return true;
    case AST_LITERAL_FLOAT:
        node->inferred_type = viper_type_float();
        return true;
    case AST_LITERAL_STRING:
        node->inferred_type = viper_type_string();
        return true;
    case AST_LITERAL_BOOL:
        node->inferred_type = viper_type_bool();
        return true;
    case AST_LITERAL_TENSOR:
        return infer_tensor_literal(ctx, node, &node->inferred_type);
    case AST_IDENTIFIER: {
        Symbol *sym = symtab_lookup(ctx->symtab, node->as.identifier.name);
        if (!sym) {
            type_error(ctx, node, "undeclared identifier '%s'", node->as.identifier.name);
            return false;
        }
        node->inferred_type = sym->type;
        return true;
    }
    case AST_QUALIFIED_NAME: {
        Symbol *sym = symtab_lookup_qualified(ctx->symtab, node->as.qualified_name.module, node->as.qualified_name.name);
        if (!sym) {
            type_error(ctx, node, "undeclared identifier '%s.%s'", node->as.qualified_name.module,
                       node->as.qualified_name.name);
            return false;
        }
        node->inferred_type = sym->type;
        return true;
    }
    case AST_UNARY:
        if (!check_expr(ctx, node->as.unary.operand)) {
            return false;
        }
        if (node->as.unary.operand->inferred_type.kind != TYPE_INT &&
            node->as.unary.operand->inferred_type.kind != TYPE_FLOAT) {
            type_error(ctx, node, "unary '-' requires numeric operand");
            return false;
        }
        node->inferred_type = node->as.unary.operand->inferred_type;
        return true;
    case AST_BINARY: {
        if (!check_expr(ctx, node->as.binary.left) || !check_expr(ctx, node->as.binary.right)) {
            return false;
        }
        ViperType lt = node->as.binary.left->inferred_type;
        ViperType rt = node->as.binary.right->inferred_type;
        BinOp op = node->as.binary.op;

        if (op == OP_MATMUL) {
            ViperType result;
            if (!tensor_matmul_result(&lt, &rt, &result)) {
                if (lt.kind == TYPE_TENSOR && rt.kind == TYPE_TENSOR && lt.shape.rank == 2 && rt.shape.rank == 2) {
                    type_error(ctx, node,
                               "matrix multiply shape mismatch: (%d,%d) @ (%d,%d) — inner dimensions %d and %d do not match",
                               lt.shape.dims[0], lt.shape.dims[1], rt.shape.dims[0], rt.shape.dims[1],
                               lt.shape.dims[1], rt.shape.dims[0]);
                } else {
                    type_error(ctx, node, "matrix multiply requires two 2D tensors with matching element types");
                }
                return false;
            }
            node->inferred_type = result;
            return true;
        }

        if (op == OP_EQ || op == OP_NEQ || op == OP_LT || op == OP_GT || op == OP_LTE || op == OP_GTE) {
            if (!viper_type_eq(&lt, &rt)) {
                type_error(ctx, node, "comparison operands must have the same type");
                return false;
            }
            node->inferred_type = viper_type_bool();
            return true;
        }

        if (op == OP_ADD && lt.kind == TYPE_STRING && rt.kind == TYPE_STRING) {
            node->inferred_type = viper_type_string();
            return true;
        }

        if (lt.kind == TYPE_TENSOR && rt.kind == TYPE_TENSOR) {
            ViperType result;
            if (!tensor_elementwise_result(&lt, &rt, &result)) {
                type_error(ctx, node, "tensor element-wise operands must have the same shape and element type");
                return false;
            }
            if (op != OP_ADD && op != OP_SUB && op != OP_MUL && op != OP_DIV) {
                type_error(ctx, node, "unsupported element-wise tensor operator");
                return false;
            }
            node->inferred_type = result;
            return true;
        }

        if (!viper_type_eq(&lt, &rt)) {
            type_error(ctx, node, "binary operands must have the same type");
            return false;
        }
        if (lt.kind != TYPE_INT && lt.kind != TYPE_FLOAT) {
            type_error(ctx, node, "arithmetic requires int or float operands");
            return false;
        }
        if (op == OP_ADD || op == OP_SUB || op == OP_MUL || op == OP_DIV) {
            node->inferred_type = lt;
            return true;
        }
        type_error(ctx, node, "unsupported binary operator");
        return false;
    }
    case AST_CAST: {
        if (!check_expr(ctx, node->as.cast.operand)) {
            return false;
        }
        TypeKind from = node->as.cast.operand->inferred_type.kind;
        TypeKind to = node->as.cast.target.kind;
        if ((to == TYPE_INT || to == TYPE_FLOAT || to == TYPE_BOOL) &&
            (from == TYPE_INT || from == TYPE_FLOAT || from == TYPE_BOOL)) {
            node->inferred_type = node->as.cast.target;
            return true;
        }
        type_error(ctx, node, "invalid cast");
        return false;
    }
    case AST_INDEX: {
        if (!check_expr(ctx, node->as.index.base)) {
            return false;
        }
        ViperType base = node->as.index.base->inferred_type;
        if (base.kind != TYPE_TENSOR) {
            type_error(ctx, node, "index operator requires a tensor");
            return false;
        }
        if (node->as.index.index_count != base.shape.rank) {
            type_error(ctx, node, "index count must match tensor rank");
            return false;
        }
        for (int i = 0; i < node->as.index.index_count; i++) {
            if (!check_expr(ctx, node->as.index.indices[i])) {
                return false;
            }
            if (node->as.index.indices[i]->inferred_type.kind != TYPE_INT) {
                type_error(ctx, node, "tensor index must be int");
                return false;
            }
        }
        node->inferred_type = base.elem == TYPE_INT ? viper_type_int() : viper_type_float();
        return true;
    }
    case AST_CALL:
        return check_call(ctx, node);
    default:
        type_error(ctx, node, "invalid expression");
        return false;
    }
}

static bool check_var_decl(SemanticContext *ctx, AstNode *node) {
    if (!check_expr(ctx, node->as.var_decl.value)) {
        return false;
    }
    if (!viper_type_eq(&node->as.var_decl.type, &node->as.var_decl.value->inferred_type)) {
        char expected[64], got[64];
        viper_type_to_string(&node->as.var_decl.type, expected, sizeof(expected));
        viper_type_to_string(&node->as.var_decl.value->inferred_type, got, sizeof(got));
        type_error(ctx, node, "type mismatch in declaration: expected %s, got %s", expected, got);
        return false;
    }

    Symbol *sym = symbol_create(node->as.var_decl.name, SYM_VAR, node->as.var_decl.type, node->line, node->column);
    if (!symtab_insert(ctx->symtab, sym)) {
        type_error(ctx, node, "redeclaration of '%s'", node->as.var_decl.name);
        symbol_free_local(sym);
        return false;
    }
    return true;
}

static bool check_assign(SemanticContext *ctx, AstNode *node) {
    Symbol *sym = symtab_lookup(ctx->symtab, node->as.assign.name);
    if (!sym) {
        type_error(ctx, node, "undeclared identifier '%s'", node->as.assign.name);
        return false;
    }
    if (!check_expr(ctx, node->as.assign.value)) {
        return false;
    }
    if (!viper_type_eq(&sym->type, &node->as.assign.value->inferred_type)) {
        char expected[64], got[64];
        viper_type_to_string(&sym->type, expected, sizeof(expected));
        viper_type_to_string(&node->as.assign.value->inferred_type, got, sizeof(got));
        type_error(ctx, node, "assignment type mismatch: expected %s, got %s", expected, got);
        return false;
    }
    return true;
}

static bool check_block(SemanticContext *ctx, AstNode *node) {
    symtab_push_scope(ctx->symtab);
    bool ok = true;
    for (int i = 0; i < node->as.block.statement_count; i++) {
        if (!check_stmt(ctx, node->as.block.statements[i])) {
            ok = false;
        }
    }
    symtab_pop_scope(ctx->symtab);
    return ok;
}

static bool check_fn_body(SemanticContext *ctx, AstNode *node) {
    symtab_push_scope(ctx->symtab);
    for (int i = 0; i < node->as.fn_decl.param_count; i++) {
        Symbol *ps = symbol_create(node->as.fn_decl.params[i].name, SYM_VAR, node->as.fn_decl.params[i].type,
                                   node->line, node->column);
        if (!symtab_insert(ctx->symtab, ps)) {
            type_error(ctx, node, "duplicate parameter '%s'", node->as.fn_decl.params[i].name);
            symbol_free_local(ps);
        }
    }

    ViperType saved_return = ctx->current_return_type;
    bool saved_in_fn = ctx->in_function;
    ctx->current_return_type = node->as.fn_decl.return_type;
    ctx->in_function = true;

    bool ok = check_block(ctx, node->as.fn_decl.body);

    ctx->current_return_type = saved_return;
    ctx->in_function = saved_in_fn;
    symtab_pop_scope(ctx->symtab);
    return ok;
}

static bool check_open_stmt(SemanticContext *ctx, AstNode *node) {
    const char *mod = node->as.open_stmt.module_name;
    if (ctx->module_index && !module_index_find(ctx->module_index, mod)) {
        type_error(ctx, node, "unknown module '%s'", mod);
        return false;
    }
    for (int i = 0; i < ctx->open_count; i++) {
        if (strcmp(ctx->opened_modules[i], mod) == 0) {
            return true;
        }
    }
    char **opens = (char **)realloc(ctx->opened_modules, (size_t)(ctx->open_count + 1) * sizeof(char *));
    if (!opens) {
        return false;
    }
    ctx->opened_modules = opens;
    ctx->opened_modules[ctx->open_count] = sem_strdup(ctx, mod);
    if (!ctx->opened_modules[ctx->open_count]) {
        return false;
    }
    ctx->open_count++;
    return true;
}

static bool check_stmt(SemanticContext *ctx, AstNode *node) {
    if (!node) {
        return false;
    }

    switch (node->kind) {
    case AST_MODULE_DECL:
        return true;
    case AST_OPEN_STMT:
        return check_open_stmt(ctx, node);
    case AST_VAR_DECL:
        return check_var_decl(ctx, node);
    case AST_ASSIGN:
        return check_assign(ctx, node);
    case AST_IF:
        if (!check_expr(ctx, node->as.if_stmt.condition)) {
            return false;
        }
        if (node->as.if_stmt.condition->inferred_type.kind != TYPE_BOOL) {
            type_error(ctx, node, "if condition must be bool");
            return false;
        }
        return check_block(ctx, node->as.if_stmt.then_branch) &&
               (node->as.if_stmt.else_branch ? check_block(ctx, node->as.if_stmt.else_branch) : true);
    case AST_WHILE:
        if (!check_expr(ctx, node->as.while_stmt.condition)) {
            return false;
        }
        if (node->as.while_stmt.condition->inferred_type.kind != TYPE_BOOL) {
            type_error(ctx, node, "while condition must be bool");
            return false;
        }
        return check_block(ctx, node->as.while_stmt.body);
    case AST_FOR:
        if (!check_expr(ctx, node->as.for_stmt.start) || !check_expr(ctx, node->as.for_stmt.end)) {
            return false;
        }
        if (node->as.for_stmt.start->inferred_type.kind != TYPE_INT ||
            node->as.for_stmt.end->inferred_type.kind != TYPE_INT) {
            type_error(ctx, node, "for loop range bounds must be int");
            return false;
        }
        symtab_push_scope(ctx->symtab);
        Symbol *loop_var = symbol_create(node->as.for_stmt.name, SYM_VAR, viper_type_int(), node->line, node->column);
        symtab_insert(ctx->symtab, loop_var);
        bool ok = check_block(ctx, node->as.for_stmt.body);
        symtab_pop_scope(ctx->symtab);
        return ok;
    case AST_FN_DECL:
        return check_fn_body(ctx, node);
    case AST_RETURN:
        if (!ctx->in_function) {
            type_error(ctx, node, "return outside function");
            return false;
        }
        if (!check_expr(ctx, node->as.return_stmt.value)) {
            return false;
        }
        if (!viper_type_eq(&ctx->current_return_type, &node->as.return_stmt.value->inferred_type)) {
            char expected[64], got[64];
            viper_type_to_string(&ctx->current_return_type, expected, sizeof(expected));
            viper_type_to_string(&node->as.return_stmt.value->inferred_type, got, sizeof(got));
            type_error(ctx, node, "return type mismatch: expected %s, got %s", expected, got);
            return false;
        }
        return true;
    case AST_BLOCK:
        return check_block(ctx, node);
    case AST_EXPR_STMT:
        return check_expr(ctx, node->as.expr_stmt.expr);
    case AST_PRINT:
        if (!check_expr(ctx, node->as.print_stmt.expr)) {
            return false;
        }
        switch (node->as.print_stmt.expr->inferred_type.kind) {
        case TYPE_INT:
        case TYPE_FLOAT:
        case TYPE_STRING:
        case TYPE_BOOL:
        case TYPE_TENSOR:
            return true;
        default:
            type_error(ctx, node, "cannot print this type");
            return false;
        }
    default:
        return check_expr(ctx, node);
    }
}

bool semantic_register_fn(SemanticContext *ctx, AstNode *fn_node) {
    if (!fn_node || fn_node->kind != AST_FN_DECL) {
        return false;
    }
    if (!fn_node->as.fn_decl.exported) {
        return true;
    }

    const char *mod = fn_node->as.fn_decl.module_name ? fn_node->as.fn_decl.module_name : ctx->current_module;
    if (!mod) {
        type_error(ctx, fn_node, "exported function '%s' requires a module name", fn_node->as.fn_decl.name);
        return false;
    }

    Symbol *existing = symtab_lookup_qualified(ctx->symtab, mod, fn_node->as.fn_decl.name);
    if (existing) {
        type_error(ctx, fn_node, "function '%s.%s' already registered at %d:%d", mod, fn_node->as.fn_decl.name,
                   existing->line, existing->column);
        return false;
    }

    Symbol *sym = symbol_create(fn_node->as.fn_decl.name, SYM_FN, fn_node->as.fn_decl.return_type, fn_node->line,
                                fn_node->column);
    if (!sym) {
        return false;
    }

    sym->param_count = fn_node->as.fn_decl.param_count;
    if (sym->param_count > 0) {
        sym->param_types = (ViperType *)calloc((size_t)sym->param_count, sizeof(ViperType));
        sym->param_names = (char **)calloc((size_t)sym->param_count, sizeof(char *));
        for (int i = 0; i < sym->param_count; i++) {
            sym->param_types[i] = fn_node->as.fn_decl.params[i].type;
            sym->param_names[i] = strdup(fn_node->as.fn_decl.params[i].name);
        }
    }

    if (!symtab_register_exported(ctx->symtab, mod, sym)) {
        type_error(ctx, fn_node, "failed to register function '%s.%s'", mod, fn_node->as.fn_decl.name);
        symbol_free_local(sym);
        return false;
    }
    return true;
}

bool semantic_check_program(SemanticContext *ctx, AstNode *program) {
    if (!program || program->kind != AST_PROGRAM) {
        return false;
    }

    ctx->current_module = program->as.program.module_name;
    ctx->open_count = 0;
    ctx->opened_modules = NULL;

    symtab_push_scope(ctx->symtab);
    for (int i = 0; i < program->as.program.statement_count; i++) {
        AstNode *stmt = program->as.program.statements[i];
        if (stmt->kind == AST_FN_DECL) {
            register_file_function(ctx, stmt);
        }
    }

    bool ok = true;
    for (int i = 0; i < program->as.program.statement_count; i++) {
        if (!check_stmt(ctx, program->as.program.statements[i])) {
            ok = false;
        }
    }

    symtab_pop_scope(ctx->symtab);
    return ok && ctx->diag->error_count == 0;
}
