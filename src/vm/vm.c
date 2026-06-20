#include "vm.h"

#include <alloca.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct VarSlot {
    char *name;
    VpValue value;
} VarSlot;

typedef struct Vm {
    BcProgram *prog;
    DiagContext *diag;
    VpValue *stack;
    int stack_cap;
    int sp;
    VarSlot *globals;
    int global_count;
    int global_cap;
    int *call_stack;
    int call_sp;
    int call_cap;
} Vm;

static void vm_runtime_error(Vm *vm, BcInstr *instr, const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    diag_emit(vm->diag, DIAG_ERROR, NULL, instr ? instr->line : 0, instr ? instr->column : 0, "%s", buf);
}

static bool vm_push(Vm *vm, VpValue val) {
    if (vm->sp >= vm->stack_cap) {
        int cap = vm->stack_cap == 0 ? 64 : vm->stack_cap * 2;
        VpValue *stack = (VpValue *)realloc(vm->stack, (size_t)cap * sizeof(VpValue));
        if (!stack) {
            return false;
        }
        vm->stack = stack;
        vm->stack_cap = cap;
    }
    vm->stack[vm->sp++] = val;
    return true;
}

static bool vm_pop(Vm *vm, VpValue *out) {
    if (vm->sp == 0) {
        return false;
    }
    *out = vm->stack[--vm->sp];
    return true;
}

static VarSlot *vm_find_var(Vm *vm, const char *name) {
    for (int i = vm->global_count - 1; i >= 0; i--) {
        if (strcmp(vm->globals[i].name, name) == 0) {
            return &vm->globals[i];
        }
    }
    return NULL;
}

static bool vm_set_var(Vm *vm, const char *name, VpValue val, bool declare) {
    VarSlot *slot = vm_find_var(vm, name);
    if (slot) {
        vp_value_free(&slot->value);
        slot->value = vp_value_clone(&val);
        vp_value_free(&val);
        return true;
    }
    if (!declare) {
        vp_value_free(&val);
        return false;
    }
    if (vm->global_count >= vm->global_cap) {
        int cap = vm->global_cap == 0 ? 16 : vm->global_cap * 2;
        VarSlot *globals = (VarSlot *)realloc(vm->globals, (size_t)cap * sizeof(VarSlot));
        if (!globals) {
            vp_value_free(&val);
            return false;
        }
        vm->globals = globals;
        vm->global_cap = cap;
    }
    vm->globals[vm->global_count].name = strdup(name);
    vm->globals[vm->global_count].value = val;
    vm->global_count++;
    return true;
}

static bool vm_binary(Vm *vm, BinOp op, BcInstr *instr) {
    VpValue right, left;
    if (!vm_pop(vm, &right) || !vm_pop(vm, &left)) {
        vm_runtime_error(vm, instr, "stack underflow");
        vp_value_free(&right);
        vp_value_free(&left);
        return false;
    }
    VpValue result = {viper_type_unknown(), {0}};
    if (left.type.kind == TYPE_STRING && right.type.kind == TYPE_STRING && op == OP_ADD) {
        size_t len = strlen(left.as.str ? left.as.str : "") + strlen(right.as.str ? right.as.str : "") + 1;
        char *buf = (char *)malloc(len);
        snprintf(buf, len, "%s%s", left.as.str ? left.as.str : "", right.as.str ? right.as.str : "");
        result = vp_value_string(buf);
        free(buf);
    } else if (left.type.kind == TYPE_INT && right.type.kind == TYPE_INT) {
        long long l = left.as.i, r = right.as.i;
        switch (op) {
        case OP_ADD: result = vp_value_int(l + r); break;
        case OP_SUB: result = vp_value_int(l - r); break;
        case OP_MUL: result = vp_value_int(l * r); break;
        case OP_DIV:
            if (r == 0) {
                vm_runtime_error(vm, instr, "division by zero");
                break;
            }
            result = vp_value_int(l / r);
            break;
        case OP_EQ: result = vp_value_bool(l == r); break;
        case OP_NEQ: result = vp_value_bool(l != r); break;
        case OP_LT: result = vp_value_bool(l < r); break;
        case OP_GT: result = vp_value_bool(l > r); break;
        case OP_LTE: result = vp_value_bool(l <= r); break;
        case OP_GTE: result = vp_value_bool(l >= r); break;
        default: vm_runtime_error(vm, instr, "invalid binary op"); break;
        }
    } else if (left.type.kind == TYPE_FLOAT && right.type.kind == TYPE_FLOAT) {
        double l = left.as.f, r = right.as.f;
        switch (op) {
        case OP_ADD: result = vp_value_float(l + r); break;
        case OP_SUB: result = vp_value_float(l - r); break;
        case OP_MUL: result = vp_value_float(l * r); break;
        case OP_DIV:
            if (r == 0.0) {
                vm_runtime_error(vm, instr, "division by zero");
                break;
            }
            result = vp_value_float(l / r);
            break;
        case OP_EQ: result = vp_value_bool(l == r); break;
        case OP_NEQ: result = vp_value_bool(l != r); break;
        case OP_LT: result = vp_value_bool(l < r); break;
        case OP_GT: result = vp_value_bool(l > r); break;
        case OP_LTE: result = vp_value_bool(l <= r); break;
        case OP_GTE: result = vp_value_bool(l >= r); break;
        default: vm_runtime_error(vm, instr, "invalid binary op"); break;
        }
    } else if (left.type.kind == TYPE_BOOL && right.type.kind == TYPE_BOOL &&
               (op == OP_EQ || op == OP_NEQ)) {
        result = op == OP_EQ ? vp_value_bool(left.as.b == right.as.b) : vp_value_bool(left.as.b != right.as.b);
    } else {
        vm_runtime_error(vm, instr, "type mismatch in binary operation");
    }
    vp_value_free(&left);
    vp_value_free(&right);
    if (result.type.kind == TYPE_UNKNOWN) {
        vp_value_free(&result);
        return false;
    }
    return vm_push(vm, result);
}

static int vm_find_function(BcProgram *prog, int name_idx) {
    for (int i = 0; i < prog->fn_count; i++) {
        if (prog->functions[i].name_idx == name_idx) {
            return i;
        }
    }
    return -1;
}

static bool vm_execute(Vm *vm, int start_pc) {
    BcProgram *prog = vm->prog;
    int pc = start_pc;
    while (pc < prog->code_count) {
        BcInstr *instr = &prog->code[pc++];
        switch (instr->op) {
        case BC_PUSH_CONST:
            if (!vm_push(vm, vp_value_clone(&prog->constants[instr->as.const_idx]))) {
                return false;
            }
            break;
        case BC_LOAD: {
            const char *name = prog->names[instr->as.name_idx];
            VarSlot *slot = vm_find_var(vm, name);
            if (!slot) {
                vm_runtime_error(vm, instr, "undefined variable '%s'", name);
                return false;
            }
            vm_push(vm, vp_value_clone(&slot->value));
            break;
        }
        case BC_DECLARE:
        case BC_STORE: {
            VpValue val;
            if (!vm_pop(vm, &val)) {
                vm_runtime_error(vm, instr, "stack underflow");
                return false;
            }
            const char *name = prog->names[instr->as.name_idx];
            if (!vm_set_var(vm, name, val, instr->op == BC_DECLARE)) {
                vm_runtime_error(vm, instr, instr->op == BC_DECLARE ? "redeclaration" : "undefined variable");
                return false;
            }
            break;
        }
        case BC_BINARY:
            if (!vm_binary(vm, instr->as.binary.binop, instr)) {
                return false;
            }
            break;
        case BC_TENSOR_BINOP: {
            VpValue right, left;
            if (!vm_pop(vm, &right) || !vm_pop(vm, &left)) {
                vm_runtime_error(vm, instr, "stack underflow");
                return false;
            }
            VpTensor *t = vp_tensor_elementwise_binop(instr->as.tensor_binop.binop, left.as.tensor, right.as.tensor);
            vp_value_free(&left);
            vp_value_free(&right);
            if (!t) {
                vm_runtime_error(vm, instr, "tensor binop failed");
                return false;
            }
            vm_push(vm, vp_value_tensor(t));
            break;
        }
        case BC_MATMUL: {
            VpValue right, left;
            if (!vm_pop(vm, &right) || !vm_pop(vm, &left)) {
                vm_runtime_error(vm, instr, "stack underflow");
                return false;
            }
            VpTensor *t = vp_tensor_matmul(left.as.tensor, right.as.tensor);
            vp_value_free(&left);
            vp_value_free(&right);
            if (!t) {
                vm_runtime_error(vm, instr, "matmul failed");
                return false;
            }
            vm_push(vm, vp_value_tensor(t));
            break;
        }
        case BC_CAST: {
            VpValue val;
            if (!vm_pop(vm, &val)) {
                vm_runtime_error(vm, instr, "stack underflow");
                return false;
            }
            VpValue out = vp_value_cast(&val, instr->as.cast.target);
            vp_value_free(&val);
            if (out.type.kind == TYPE_UNKNOWN) {
                vm_runtime_error(vm, instr, "invalid cast");
                return false;
            }
            vm_push(vm, out);
            break;
        }
        case BC_INDEX: {
            int n = instr->as.index.index_count;
            VpValue *indices = (VpValue *)alloca((size_t)n * sizeof(VpValue));
            for (int i = n - 1; i >= 0; i--) {
                if (!vm_pop(vm, &indices[i])) {
                    vm_runtime_error(vm, instr, "stack underflow");
                    return false;
                }
            }
            VpValue base;
            if (!vm_pop(vm, &base)) {
                vm_runtime_error(vm, instr, "stack underflow");
                return false;
            }
            int idx_arr[8];
            for (int i = 0; i < n; i++) {
                idx_arr[i] = (int)indices[i].as.i;
                vp_value_free(&indices[i]);
            }
            if (base.type.elem == TYPE_INT) {
                long long v;
                vp_tensor_get(base.as.tensor, idx_arr, &v);
                vp_value_free(&base);
                vm_push(vm, vp_value_int(v));
            } else {
                double v;
                vp_tensor_get(base.as.tensor, idx_arr, &v);
                vp_value_free(&base);
                vm_push(vm, vp_value_float(v));
            }
            break;
        }
        case BC_PRINT: {
            VpValue val;
            if (!vm_pop(vm, &val)) {
                vm_runtime_error(vm, instr, "stack underflow");
                return false;
            }
            vp_value_print(&val, stdout);
            fputc('\n', stdout);
            vp_value_free(&val);
            break;
        }
        case BC_CALL: {
            int fn_idx = vm_find_function(prog, instr->as.call.name_idx);
            if (fn_idx < 0) {
                vm_runtime_error(vm, instr, "undefined function");
                return false;
            }
            BcFunction *fn = &prog->functions[fn_idx];
            VpValue args[8];
            for (int i = fn->param_count - 1; i >= 0; i--) {
                if (!vm_pop(vm, &args[i])) {
                    vm_runtime_error(vm, instr, "stack underflow");
                    return false;
                }
            }
            for (int i = 0; i < fn->param_count; i++) {
                const char *pname = prog->names[fn->param_names[i]];
                VarSlot *slot = vm_find_var(vm, pname);
                if (slot) {
                    vp_value_free(&slot->value);
                    slot->value = vp_value_clone(&args[i]);
                } else if (!vm_set_var(vm, pname, args[i], true)) {
                    vm_runtime_error(vm, instr, "failed to bind parameter '%s'", pname);
                    for (int j = 0; j < fn->param_count; j++) {
                        vp_value_free(&args[j]);
                    }
                    return false;
                }
                vp_value_free(&args[i]);
            }
            if (vm->call_sp + 1 >= vm->call_cap) {
                int cap = vm->call_cap == 0 ? 16 : vm->call_cap * 2;
                int *cs = (int *)realloc(vm->call_stack, (size_t)cap * sizeof(int));
                if (!cs) {
                    return false;
                }
                vm->call_stack = cs;
                vm->call_cap = cap;
            }
            vm->call_stack[vm->call_sp++] = pc;
            pc = fn->entry_pc;
            break;
        }
        case BC_BUILTIN: {
            if (instr->as.builtin.kind == BUILTIN_TRANSPOSE) {
                VpValue arg;
                if (!vm_pop(vm, &arg)) {
                    return false;
                }
                VpTensor *t = vp_tensor_transpose(arg.as.tensor);
                vp_value_free(&arg);
                if (!t) {
                    vm_runtime_error(vm, instr, "transpose failed");
                    return false;
                }
                vm_push(vm, vp_value_tensor(t));
            } else if (instr->as.builtin.kind == BUILTIN_RESHAPE) {
                int n = instr->as.builtin.arg_count;
                VpValue *args = (VpValue *)alloca((size_t)n * sizeof(VpValue));
                for (int i = n - 1; i >= 0; i--) {
                    vm_pop(vm, &args[i]);
                }
                VpValue tensor_val = args[0];
                int rank = n - 1;
                int dims[8];
                for (int i = 0; i < rank; i++) {
                    dims[i] = (int)args[i + 1].as.i;
                }
                for (int i = 1; i < n; i++) {
                    vp_value_free(&args[i]);
                }
                VpTensor *t = vp_tensor_reshape(tensor_val.as.tensor, dims, rank);
                vp_value_free(&tensor_val);
                if (!t) {
                    vm_runtime_error(vm, instr, "reshape failed");
                    return false;
                }
                vm_push(vm, vp_value_tensor(t));
            }
            break;
        }
        case BC_RETURN: {
            VpValue val;
            if (!vm_pop(vm, &val)) {
                vm_runtime_error(vm, instr, "stack underflow");
                return false;
            }
            if (vm->call_sp == 0) {
                vm_push(vm, val);
                return true;
            }
            while (vm->sp > 0) {
                VpValue tmp;
                vm_pop(vm, &tmp);
                vp_value_free(&tmp);
            }
            vm_push(vm, val);
            pc = vm->call_stack[--vm->call_sp];
            break;
        }
        case BC_JUMP:
            pc = instr->as.jump.target;
            break;
        case BC_JUMP_IF_FALSE: {
            VpValue val;
            if (!vm_pop(vm, &val)) {
                vm_runtime_error(vm, instr, "stack underflow");
                return false;
            }
            bool truthy = val.type.kind == TYPE_BOOL ? val.as.b : val.as.i != 0;
            vp_value_free(&val);
            if (!truthy) {
                pc = instr->as.jump.target;
            }
            break;
        }
        case BC_POP: {
            VpValue val;
            vm_pop(vm, &val);
            vp_value_free(&val);
            break;
        }
        case BC_HALT:
            return true;
        default:
            vm_runtime_error(vm, instr, "unknown opcode");
            return false;
        }
    }
    return true;
}

bool vm_run(BcProgram *prog, DiagContext *diag) {
    Vm vm = {0};
    vm.prog = prog;
    vm.diag = diag;
    bool ok = vm_execute(&vm, 0);
    for (int i = 0; i < vm.global_count; i++) {
        free(vm.globals[i].name);
        vp_value_free(&vm.globals[i].value);
    }
    free(vm.globals);
    for (int i = 0; i < vm.sp; i++) {
        vp_value_free(&vm.stack[i]);
    }
    free(vm.stack);
    free(vm.call_stack);
    return ok && diag->error_count == 0;
}
