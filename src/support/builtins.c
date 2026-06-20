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
    Symbol *transpose = make_fn("transpose", any_tensor, params1, 1, 0, 0);
    symtab_register_global(st, transpose);

    ViperType params_reshape[] = {any_tensor, viper_type_int(), viper_type_int()};
    Symbol *reshape = make_fn("reshape", any_tensor, params_reshape, 3, 0, 0);
    symtab_register_global(st, reshape);
}

bool builtins_is_transpose(const char *name) {
    return strcmp(name, "transpose") == 0;
}

bool builtins_is_reshape(const char *name) {
    return strcmp(name, "reshape") == 0;
}

bool builtins_is(const char *name) {
    return builtins_is_transpose(name) || builtins_is_reshape(name);
}
