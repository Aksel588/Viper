#include "builtins.h"

#include <stdlib.h>
#include <string.h>

static Symbol *make_fn(const char *name, ViperType ret, ViperType *params, int param_count, int line, int col) {
    Symbol *sym = symbol_create(name, SYM_FN, ret, line, col);
    if (!sym) {
        return NULL;
    }
    sym->param_count = param_count;
    if (param_count > 0) {
        sym->param_types = (ViperType *)malloc((size_t)param_count * sizeof(ViperType));
        for (int i = 0; i < param_count; i++) {
            sym->param_types[i] = params[i];
        }
    }
    return sym;
}

void builtins_register(SymbolTable *st) {
    ViperType any_tensor = viper_type_tensor(TYPE_FLOAT, (int[]){0, 0}, 2);
    any_tensor.shape.dims[0] = 0;
    any_tensor.shape.dims[1] = 0;

    ViperType params1[] = {any_tensor};
    symtab_register_global(st, make_fn("transpose", any_tensor, params1, 1, 0, 0));

    ViperType params_reshape[] = {any_tensor, viper_type_int(), viper_type_int()};
    symtab_register_global(st, make_fn("reshape", any_tensor, params_reshape, 3, 0, 0));

    ViperType any_list = viper_type_list(TYPE_INT);
    ViperType str_type = viper_type_string();
    ViperType int_type = viper_type_int();
    ViperType float_type = viper_type_float();

    symtab_register_global(st, make_fn("len", int_type, (ViperType[]){str_type}, 1, 0, 0));
    symtab_register_global(st, make_fn("len", int_type, (ViperType[]){any_list}, 1, 0, 0));
    symtab_register_global(st, make_fn("append", any_list, (ViperType[]){any_list, int_type}, 2, 0, 0));
    symtab_register_global(st, make_fn("abs", int_type, (ViperType[]){int_type}, 1, 0, 0));
    symtab_register_global(st, make_fn("abs", float_type, (ViperType[]){float_type}, 1, 0, 0));
    symtab_register_global(st, make_fn("sqrt", float_type, (ViperType[]){float_type}, 1, 0, 0));
    symtab_register_global(st, make_fn("floor", float_type, (ViperType[]){float_type}, 1, 0, 0));
    symtab_register_global(st, make_fn("ceil", float_type, (ViperType[]){float_type}, 1, 0, 0));
    symtab_register_global(st, make_fn("read_file", str_type, (ViperType[]){str_type}, 1, 0, 0));
    symtab_register_global(st, make_fn("write_file", int_type, (ViperType[]){str_type, str_type}, 2, 0, 0));
    symtab_register_global(st, make_fn("input", str_type, NULL, 0, 0, 0));
}

bool builtins_is_transpose(const char *name) { return strcmp(name, "transpose") == 0; }
bool builtins_is_reshape(const char *name) { return strcmp(name, "reshape") == 0; }
bool builtins_is_len(const char *name) { return strcmp(name, "len") == 0; }
bool builtins_is_append(const char *name) { return strcmp(name, "append") == 0; }
bool builtins_is_abs(const char *name) { return strcmp(name, "abs") == 0; }
bool builtins_is_sqrt(const char *name) { return strcmp(name, "sqrt") == 0; }
bool builtins_is_floor(const char *name) { return strcmp(name, "floor") == 0; }
bool builtins_is_ceil(const char *name) { return strcmp(name, "ceil") == 0; }
bool builtins_is_read_file(const char *name) { return strcmp(name, "read_file") == 0; }
bool builtins_is_write_file(const char *name) { return strcmp(name, "write_file") == 0; }
bool builtins_is_input(const char *name) { return strcmp(name, "input") == 0; }

bool builtins_is(const char *name) {
    return builtins_is_transpose(name) || builtins_is_reshape(name) || builtins_is_len(name) ||
           builtins_is_append(name) || builtins_is_abs(name) || builtins_is_sqrt(name) ||
           builtins_is_floor(name) || builtins_is_ceil(name) || builtins_is_read_file(name) ||
           builtins_is_write_file(name) || builtins_is_input(name);
}
