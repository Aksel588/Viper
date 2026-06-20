#ifndef VIPER_BYTECODE_H
#define VIPER_BYTECODE_H

#include <stdbool.h>

#include "ast.h"
#include "diagnostics.h"
#include "runtime.h"

#define BC_MAGIC "VBC\001"

typedef enum {
    BC_PUSH_CONST,
    BC_LOAD,
    BC_STORE,
    BC_DECLARE,
    BC_BINARY,
    BC_TENSOR_BINOP,
    BC_MATMUL,
    BC_CAST,
    BC_INDEX,
    BC_PRINT,
    BC_CALL,
    BC_BUILTIN,
    BC_RETURN,
    BC_JUMP,
    BC_JUMP_IF_FALSE,
    BC_POP,
    BC_HALT
} BcOp;

typedef enum {
    BUILTIN_TRANSPOSE,
    BUILTIN_RESHAPE
} BcBuiltin;

typedef struct BcInstr {
    BcOp op;
    int line;
    int column;
    union {
        int const_idx;
        int name_idx;
        struct {
            BinOp binop;
        } binary;
        struct {
            RtBinOp binop;
        } tensor_binop;
        struct {
            TypeKind target;
        } cast;
        struct {
            int index_count;
        } index;
        struct {
            int name_idx;
            int arg_count;
        } call;
        struct {
            BcBuiltin kind;
            int arg_count;
        } builtin;
        struct {
            int target;
        } jump;
    } as;
} BcInstr;

typedef struct BcFunction {
    int name_idx;
    int entry_pc;
    int param_count;
    int param_names[8];
    ViperType return_type;
} BcFunction;

typedef struct BcProgram {
    BcInstr *code;
    int code_count;
    int code_capacity;
    char **names;
    int name_count;
    int name_capacity;
    VpValue *constants;
    int const_count;
    int const_capacity;
    BcFunction *functions;
    int fn_count;
    int fn_capacity;
} BcProgram;

void bc_program_init(BcProgram *prog);
void bc_program_free(BcProgram *prog);
bool bc_compile_program(AstNode *program, AstNode **extra_fns, int extra_fn_count, BcProgram *out,
                        DiagContext *diag);
bool bc_write_file(const BcProgram *prog, const char *path);
bool bc_read_file(BcProgram *prog, const char *path);

#endif
