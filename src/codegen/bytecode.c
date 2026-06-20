#include "bytecode.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct BcCompiler {
    BcProgram *prog;
    DiagContext *diag;
    const char *file;
    bool compiling_fn;
} BcCompiler;

static int bc_add_name(BcProgram *prog, const char *name) {
    for (int i = 0; i < prog->name_count; i++) {
        if (strcmp(prog->names[i], name) == 0) {
            return i;
        }
    }
    if (prog->name_count >= prog->name_capacity) {
        int cap = prog->name_capacity == 0 ? 16 : prog->name_capacity * 2;
        char **names = (char **)realloc(prog->names, (size_t)cap * sizeof(char *));
        if (!names) {
            return -1;
        }
        prog->names = names;
        prog->name_capacity = cap;
    }
    prog->names[prog->name_count] = strdup(name);
    if (!prog->names[prog->name_count]) {
        return -1;
    }
    return prog->name_count++;
}

static int bc_add_const(BcProgram *prog, const VpValue *val) {
    if (prog->const_count >= prog->const_capacity) {
        int cap = prog->const_capacity == 0 ? 16 : prog->const_capacity * 2;
        VpValue *constants = (VpValue *)realloc(prog->constants, (size_t)cap * sizeof(VpValue));
        if (!constants) {
            return -1;
        }
        prog->constants = constants;
        prog->const_capacity = cap;
    }
    prog->constants[prog->const_count] = vp_value_clone(val);
    return prog->const_count++;
}

static int bc_emit(BcCompiler *c, BcOp op, int line, int column) {
    BcProgram *prog = c->prog;
    if (prog->code_count >= prog->code_capacity) {
        int cap = prog->code_capacity == 0 ? 64 : prog->code_capacity * 2;
        BcInstr *code = (BcInstr *)realloc(prog->code, (size_t)cap * sizeof(BcInstr));
        if (!code) {
            return -1;
        }
        prog->code = code;
        prog->code_capacity = cap;
    }
    int pc = prog->code_count;
    BcInstr *instr = &prog->code[prog->code_count++];
    memset(instr, 0, sizeof(*instr));
    instr->op = op;
    instr->line = line;
    instr->column = column;
    return pc;
}

static RtBinOp binop_to_rt(BinOp op) {
    switch (op) {
    case OP_ADD: return RT_OP_ADD;
    case OP_SUB: return RT_OP_SUB;
    case OP_MUL: return RT_OP_MUL;
    case OP_DIV: return RT_OP_DIV;
    default: return RT_OP_ADD;
    }
}

static VpValue ast_to_const(AstNode *node) {
    switch (node->kind) {
    case AST_LITERAL_INT:
        return vp_value_int(node->as.literal_int.value);
    case AST_LITERAL_FLOAT:
        return vp_value_float(node->as.literal_float.value);
    case AST_LITERAL_STRING:
        return vp_value_string(node->as.literal_string.value);
    case AST_LITERAL_BOOL:
        return vp_value_bool(node->as.literal_bool.value);
    case AST_LITERAL_TENSOR: {
        int rows = node->as.literal_tensor.row_count;
        if (rows == 0) {
            return vp_value_tensor(vp_tensor_create(&node->inferred_type));
        }
        int cols = node->as.literal_tensor.rows[0]->as.block.statement_count;
        int dims[2] = {rows, cols};
        TypeKind elem = TYPE_FLOAT;
        ViperType type = viper_type_tensor(elem, dims, 2);
        VpTensor *t = vp_tensor_create(&type);
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                AstNode *elem_node = node->as.literal_tensor.rows[r]->as.block.statements[c];
                int idx[2] = {r, c};
                if (elem_node->kind == AST_LITERAL_INT) {
                    elem = TYPE_INT;
                    long long v = elem_node->as.literal_int.value;
                    vp_tensor_set(t, idx, &v);
                } else {
                    double v = elem_node->as.literal_float.value;
                    vp_tensor_set(t, idx, &v);
                }
            }
        }
        t->type = node->inferred_type;
        return vp_value_tensor(t);
    }
    default:
        return (VpValue){viper_type_unknown(), {0}};
    }
}

static bool compile_expr(BcCompiler *c, AstNode *node);
static bool compile_stmt(BcCompiler *c, AstNode *node);

static bool compile_expr(BcCompiler *c, AstNode *node) {
    if (!node) {
        return false;
    }

    switch (node->kind) {
    case AST_LITERAL_INT:
    case AST_LITERAL_FLOAT:
    case AST_LITERAL_STRING:
    case AST_LITERAL_BOOL:
    case AST_LITERAL_TENSOR: {
        VpValue val = ast_to_const(node);
        int idx = bc_add_const(c->prog, &val);
        vp_value_free(&val);
        if (idx < 0) {
            return false;
        }
        int pc = bc_emit(c, BC_PUSH_CONST, node->line, node->column);
        if (pc < 0) {
            return false;
        }
        c->prog->code[pc].as.const_idx = idx;
        return true;
    }
    case AST_IDENTIFIER: {
        int name_idx = bc_add_name(c->prog, node->as.identifier.name);
        if (name_idx < 0) {
            return false;
        }
        int pc = bc_emit(c, BC_LOAD, node->line, node->column);
        c->prog->code[pc].as.name_idx = name_idx;
        return true;
    }
    case AST_UNARY:
        if (!compile_expr(c, node->as.unary.operand)) {
            return false;
        }
        if (node->as.unary.op == OP_SUB) {
            VpValue zero = node->as.unary.operand->inferred_type.kind == TYPE_FLOAT
                               ? vp_value_float(0.0)
                               : vp_value_int(0);
            int idx = bc_add_const(c->prog, &zero);
            vp_value_free(&zero);
            int pc = bc_emit(c, BC_PUSH_CONST, node->line, node->column);
            c->prog->code[pc].as.const_idx = idx;
            pc = bc_emit(c, BC_BINARY, node->line, node->column);
            c->prog->code[pc].as.binary.binop = OP_SUB;
        }
        return true;
    case AST_BINARY: {
        if (!compile_expr(c, node->as.binary.left) || !compile_expr(c, node->as.binary.right)) {
            return false;
        }
        int pc;
        if (node->as.binary.op == OP_MATMUL) {
            pc = bc_emit(c, BC_MATMUL, node->line, node->column);
        } else if (node->inferred_type.kind == TYPE_TENSOR) {
            pc = bc_emit(c, BC_TENSOR_BINOP, node->line, node->column);
            c->prog->code[pc].as.tensor_binop.binop = binop_to_rt(node->as.binary.op);
        } else {
            pc = bc_emit(c, BC_BINARY, node->line, node->column);
            c->prog->code[pc].as.binary.binop = node->as.binary.op;
        }
        return pc >= 0;
    }
    case AST_CAST:
        if (!compile_expr(c, node->as.cast.operand)) {
            return false;
        }
        {
            int pc = bc_emit(c, BC_CAST, node->line, node->column);
            c->prog->code[pc].as.cast.target = node->as.cast.target.kind;
        }
        return true;
    case AST_INDEX: {
        if (!compile_expr(c, node->as.index.base)) {
            return false;
        }
        for (int i = 0; i < node->as.index.index_count; i++) {
            if (!compile_expr(c, node->as.index.indices[i])) {
                return false;
            }
        }
        int pc = bc_emit(c, BC_INDEX, node->line, node->column);
        c->prog->code[pc].as.index.index_count = node->as.index.index_count;
        return true;
    }
    case AST_CALL: {
        if (strcmp(node->as.call.name, "transpose") == 0) {
            for (int i = 0; i < node->as.call.arg_count; i++) {
                if (!compile_expr(c, node->as.call.args[i])) {
                    return false;
                }
            }
            int pc = bc_emit(c, BC_BUILTIN, node->line, node->column);
            c->prog->code[pc].as.builtin.kind = BUILTIN_TRANSPOSE;
            c->prog->code[pc].as.builtin.arg_count = node->as.call.arg_count;
            return true;
        }
        if (strcmp(node->as.call.name, "reshape") == 0) {
            for (int i = 0; i < node->as.call.arg_count; i++) {
                if (!compile_expr(c, node->as.call.args[i])) {
                    return false;
                }
            }
            int pc = bc_emit(c, BC_BUILTIN, node->line, node->column);
            c->prog->code[pc].as.builtin.kind = BUILTIN_RESHAPE;
            c->prog->code[pc].as.builtin.arg_count = node->as.call.arg_count;
            return true;
        }
        for (int i = 0; i < node->as.call.arg_count; i++) {
            if (!compile_expr(c, node->as.call.args[i])) {
                return false;
            }
        }
        char qual_name[256];
        const char *call_name = node->as.call.name;
        if (node->as.call.module) {
            snprintf(qual_name, sizeof(qual_name), "%s.%s", node->as.call.module, node->as.call.name);
            call_name = qual_name;
        }
        int name_idx = bc_add_name(c->prog, call_name);
        int pc = bc_emit(c, BC_CALL, node->line, node->column);
        c->prog->code[pc].as.call.name_idx = name_idx;
        c->prog->code[pc].as.call.arg_count = node->as.call.arg_count;
        return true;
    }
    default:
        diag_emit(c->diag, DIAG_ERROR, c->file, node->line, node->column,
                  "unsupported expression in bytecode compiler");
        return false;
    }
}

static bool compile_block(BcCompiler *c, AstNode *node) {
    for (int i = 0; i < node->as.block.statement_count; i++) {
        if (!compile_stmt(c, node->as.block.statements[i])) {
            return false;
        }
    }
    return true;
}

static bool compile_fn_decl(BcCompiler *c, AstNode *node) {
    if (!node || node->kind != AST_FN_DECL) {
        return false;
    }
    char fn_name[256];
    if (node->as.fn_decl.exported && node->as.fn_decl.module_name) {
        snprintf(fn_name, sizeof(fn_name), "%s.%s", node->as.fn_decl.module_name, node->as.fn_decl.name);
    } else {
        snprintf(fn_name, sizeof(fn_name), "%s", node->as.fn_decl.name);
    }
    int name_idx = bc_add_name(c->prog, fn_name);
    for (int i = 0; i < c->prog->fn_count; i++) {
        if (c->prog->functions[i].name_idx == name_idx) {
            return true;
        }
    }
    int jump_over = bc_emit(c, BC_JUMP, node->line, node->column);
    int entry_pc = c->prog->code_count;
    if (c->prog->fn_count >= c->prog->fn_capacity) {
        int cap = c->prog->fn_capacity == 0 ? 8 : c->prog->fn_capacity * 2;
        BcFunction *fns = (BcFunction *)realloc(c->prog->functions, (size_t)cap * sizeof(BcFunction));
        if (!fns) {
            return false;
        }
        c->prog->functions = fns;
        c->prog->fn_capacity = cap;
    }
    BcFunction fn = {0};
    fn.name_idx = name_idx;
    fn.entry_pc = entry_pc;
    fn.param_count = node->as.fn_decl.param_count;
    fn.return_type = node->as.fn_decl.return_type;
    for (int i = 0; i < node->as.fn_decl.param_count && i < 8; i++) {
        fn.param_names[i] = bc_add_name(c->prog, node->as.fn_decl.params[i].name);
    }
    c->prog->functions[c->prog->fn_count++] = fn;
    c->compiling_fn = true;
    bool ok = compile_block(c, node->as.fn_decl.body);
    bc_emit(c, BC_RETURN, node->line, node->column);
    c->compiling_fn = false;
    c->prog->code[jump_over].as.jump.target = c->prog->code_count;
    return ok;
}

static bool compile_stmt(BcCompiler *c, AstNode *node) {
    if (!node) {
        return false;
    }

    switch (node->kind) {
    case AST_VAR_DECL:
        if (!compile_expr(c, node->as.var_decl.value)) {
            return false;
        }
        {
            int name_idx = bc_add_name(c->prog, node->as.var_decl.name);
            int pc = bc_emit(c, BC_DECLARE, node->line, node->column);
            c->prog->code[pc].as.name_idx = name_idx;
        }
        return true;
    case AST_ASSIGN:
        if (!compile_expr(c, node->as.assign.value)) {
            return false;
        }
        {
            int name_idx = bc_add_name(c->prog, node->as.assign.name);
            int pc = bc_emit(c, BC_STORE, node->line, node->column);
            c->prog->code[pc].as.name_idx = name_idx;
        }
        return true;
    case AST_PRINT:
        if (!compile_expr(c, node->as.print_stmt.expr)) {
            return false;
        }
        bc_emit(c, BC_PRINT, node->line, node->column);
        return true;
    case AST_IF: {
        if (!compile_expr(c, node->as.if_stmt.condition)) {
            return false;
        }
        int jump_false = bc_emit(c, BC_JUMP_IF_FALSE, node->line, node->column);
        if (!compile_block(c, node->as.if_stmt.then_branch)) {
            return false;
        }
        if (node->as.if_stmt.else_branch) {
            int jump_end = bc_emit(c, BC_JUMP, node->line, node->column);
            c->prog->code[jump_false].as.jump.target = c->prog->code_count;
            if (!compile_block(c, node->as.if_stmt.else_branch)) {
                return false;
            }
            c->prog->code[jump_end].as.jump.target = c->prog->code_count;
        } else {
            c->prog->code[jump_false].as.jump.target = c->prog->code_count;
        }
        return true;
    }
    case AST_FOR: {
        if (!compile_expr(c, node->as.for_stmt.start)) {
            return false;
        }
        {
            int name_idx = bc_add_name(c->prog, node->as.for_stmt.name);
            int pc = bc_emit(c, BC_DECLARE, node->line, node->column);
            c->prog->code[pc].as.name_idx = name_idx;
        }
        if (!compile_expr(c, node->as.for_stmt.end)) {
            return false;
        }
        {
            int end_tmp = bc_add_name(c->prog, "__for_end");
            int pc = bc_emit(c, BC_STORE, node->line, node->column);
            c->prog->code[pc].as.name_idx = end_tmp;
        }
        int counter = bc_add_name(c->prog, node->as.for_stmt.name);
        int end_tmp = bc_add_name(c->prog, "__for_end");
        int loop_top = c->prog->code_count;
        bc_emit(c, BC_LOAD, node->line, node->column);
        c->prog->code[c->prog->code_count - 1].as.name_idx = counter;
        bc_emit(c, BC_LOAD, node->line, node->column);
        c->prog->code[c->prog->code_count - 1].as.name_idx = end_tmp;
        bc_emit(c, BC_BINARY, node->line, node->column);
        c->prog->code[c->prog->code_count - 1].as.binary.binop = OP_LTE;
        int jump_exit = bc_emit(c, BC_JUMP_IF_FALSE, node->line, node->column);
        if (!compile_block(c, node->as.for_stmt.body)) {
            return false;
        }
        bc_emit(c, BC_LOAD, node->line, node->column);
        c->prog->code[c->prog->code_count - 1].as.name_idx = counter;
        VpValue one = vp_value_int(1);
        int one_idx = bc_add_const(c->prog, &one);
        vp_value_free(&one);
        bc_emit(c, BC_PUSH_CONST, node->line, node->column);
        c->prog->code[c->prog->code_count - 1].as.const_idx = one_idx;
        bc_emit(c, BC_BINARY, node->line, node->column);
        c->prog->code[c->prog->code_count - 1].as.binary.binop = OP_ADD;
        bc_emit(c, BC_STORE, node->line, node->column);
        c->prog->code[c->prog->code_count - 1].as.name_idx = counter;
        int jump_top = bc_emit(c, BC_JUMP, node->line, node->column);
        c->prog->code[jump_top].as.jump.target = loop_top;
        c->prog->code[jump_exit].as.jump.target = c->prog->code_count;
        return true;
    }
    case AST_WHILE: {
        int loop_top = c->prog->code_count;
        if (!compile_expr(c, node->as.while_stmt.condition)) {
            return false;
        }
        int jump_exit = bc_emit(c, BC_JUMP_IF_FALSE, node->line, node->column);
        if (!compile_block(c, node->as.while_stmt.body)) {
            return false;
        }
        int jump_top = bc_emit(c, BC_JUMP, node->line, node->column);
        c->prog->code[jump_top].as.jump.target = loop_top;
        c->prog->code[jump_exit].as.jump.target = c->prog->code_count;
        return true;
    }
    case AST_MODULE_DECL:
    case AST_OPEN_STMT:
        return true;
    case AST_FN_DECL:
        return compile_fn_decl(c, node);
    case AST_RETURN:
        if (!compile_expr(c, node->as.return_stmt.value)) {
            return false;
        }
        bc_emit(c, BC_RETURN, node->line, node->column);
        return true;
    case AST_BLOCK:
        return compile_block(c, node);
    case AST_EXPR_STMT:
        if (!compile_expr(c, node->as.expr_stmt.expr)) {
            return false;
        }
        bc_emit(c, BC_POP, node->line, node->column);
        return true;
    default:
        return compile_expr(c, node);
    }
}

void bc_program_init(BcProgram *prog) {
    memset(prog, 0, sizeof(*prog));
}

void bc_program_free(BcProgram *prog) {
    for (int i = 0; i < prog->name_count; i++) {
        free(prog->names[i]);
    }
    free(prog->names);
    for (int i = 0; i < prog->const_count; i++) {
        vp_value_free(&prog->constants[i]);
    }
    free(prog->constants);
    free(prog->code);
    free(prog->functions);
    memset(prog, 0, sizeof(*prog));
}

bool bc_compile_program(AstNode *program, AstNode **extra_fns, int extra_fn_count, BcProgram *out,
                        DiagContext *diag) {
    if (!program || program->kind != AST_PROGRAM) {
        return false;
    }
    bc_program_init(out);
    BcCompiler c = {out, diag, NULL, false};

    for (int i = 0; i < extra_fn_count; i++) {
        if (!compile_fn_decl(&c, extra_fns[i])) {
            bc_program_free(out);
            return false;
        }
    }

    for (int i = 0; i < program->as.program.statement_count; i++) {
        AstNode *stmt = program->as.program.statements[i];
        if (stmt->kind == AST_FN_DECL) {
            if (!compile_fn_decl(&c, stmt)) {
                bc_program_free(out);
                return false;
            }
            continue;
        }
        if (stmt->kind == AST_MODULE_DECL || stmt->kind == AST_OPEN_STMT) {
            continue;
        }
        if (!compile_stmt(&c, stmt)) {
            bc_program_free(out);
            return false;
        }
    }
    bc_emit(&c, BC_HALT, 0, 0);
    return true;
}

bool bc_write_file(const BcProgram *prog, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        return false;
    }
    fwrite(BC_MAGIC, 1, 4, f);
    int32_t counts[4] = {prog->code_count, prog->name_count, prog->const_count, prog->fn_count};
    fwrite(counts, sizeof(counts), 1, f);
    fwrite(prog->code, sizeof(BcInstr), (size_t)prog->code_count, f);
    for (int i = 0; i < prog->name_count; i++) {
        int32_t len = (int32_t)strlen(prog->names[i]);
        fwrite(&len, sizeof(len), 1, f);
        fwrite(prog->names[i], 1, (size_t)len, f);
    }
    fwrite(prog->functions, sizeof(BcFunction), (size_t)prog->fn_count, f);
    for (int i = 0; i < prog->const_count; i++) {
        const VpValue *v = &prog->constants[i];
        fwrite(&v->type, sizeof(v->type), 1, f);
        switch (v->type.kind) {
        case TYPE_INT:
            fwrite(&v->as.i, sizeof(v->as.i), 1, f);
            break;
        case TYPE_FLOAT:
            fwrite(&v->as.f, sizeof(v->as.f), 1, f);
            break;
        case TYPE_BOOL:
            fwrite(&v->as.b, sizeof(v->as.b), 1, f);
            break;
        case TYPE_STRING: {
            int32_t len = v->as.str ? (int32_t)strlen(v->as.str) : 0;
            fwrite(&len, sizeof(len), 1, f);
            if (len > 0) {
                fwrite(v->as.str, 1, (size_t)len, f);
            }
            break;
        }
        case TYPE_TENSOR: {
            const VpTensor *t = v->as.tensor;
            fwrite(&t->type, sizeof(t->type), 1, f);
            size_t count = vp_tensor_num_elements(t);
            size_t esize = vp_tensor_elem_size(t);
            fwrite(t->data, esize, count, f);
            break;
        }
        default:
            break;
        }
    }
    fclose(f);
    return true;
}

bool bc_read_file(BcProgram *prog, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, BC_MAGIC, 4) != 0) {
        fclose(f);
        return false;
    }
    bc_program_init(prog);
    int32_t counts[4];
    if (fread(counts, sizeof(counts), 1, f) != 1) {
        fclose(f);
        return false;
    }
    prog->code_count = counts[0];
    prog->code_capacity = counts[0];
    prog->name_count = counts[1];
    prog->name_capacity = counts[1];
    prog->const_count = counts[2];
    prog->const_capacity = counts[2];
    prog->fn_count = counts[3];
    prog->fn_capacity = counts[3];
    prog->code = (BcInstr *)malloc((size_t)prog->code_count * sizeof(BcInstr));
    prog->names = (char **)calloc((size_t)prog->name_count, sizeof(char *));
    prog->constants = (VpValue *)calloc((size_t)prog->const_count, sizeof(VpValue));
    prog->functions = (BcFunction *)malloc((size_t)prog->fn_count * sizeof(BcFunction));
    if (!prog->code || !prog->names || !prog->constants || !prog->functions) {
        fclose(f);
        bc_program_free(prog);
        return false;
    }
    fread(prog->code, sizeof(BcInstr), (size_t)prog->code_count, f);
    for (int i = 0; i < prog->name_count; i++) {
        int32_t len = 0;
        fread(&len, sizeof(len), 1, f);
        prog->names[i] = (char *)malloc((size_t)len + 1);
        fread(prog->names[i], 1, (size_t)len, f);
        prog->names[i][len] = '\0';
    }
    fread(prog->functions, sizeof(BcFunction), (size_t)prog->fn_count, f);
    for (int i = 0; i < prog->const_count; i++) {
        VpValue *v = &prog->constants[i];
        fread(&v->type, sizeof(v->type), 1, f);
        switch (v->type.kind) {
        case TYPE_INT:
            fread(&v->as.i, sizeof(v->as.i), 1, f);
            break;
        case TYPE_FLOAT:
            fread(&v->as.f, sizeof(v->as.f), 1, f);
            break;
        case TYPE_BOOL:
            fread(&v->as.b, sizeof(v->as.b), 1, f);
            break;
        case TYPE_STRING: {
            int32_t len = 0;
            fread(&len, sizeof(len), 1, f);
            v->as.str = (char *)malloc((size_t)len + 1);
            fread(v->as.str, 1, (size_t)len, f);
            v->as.str[len] = '\0';
            break;
        }
        case TYPE_TENSOR: {
            ViperType type;
            fread(&type, sizeof(type), 1, f);
            VpTensor *t = vp_tensor_create(&type);
            size_t count = vp_tensor_num_elements(t);
            size_t esize = vp_tensor_elem_size(t);
            fread(t->data, esize, count, f);
            v->as.tensor = t;
            break;
        }
        default:
            break;
        }
    }
    fclose(f);
    return true;
}
