#ifndef VIPER_BUILTINS_H
#define VIPER_BUILTINS_H

#include "symbol_table.h"

void builtins_register(SymbolTable *st);
bool builtins_is(const char *name);
bool builtins_is_transpose(const char *name);
bool builtins_is_reshape(const char *name);
bool builtins_is_len(const char *name);
bool builtins_is_append(const char *name);
bool builtins_is_abs(const char *name);
bool builtins_is_sqrt(const char *name);
bool builtins_is_floor(const char *name);
bool builtins_is_ceil(const char *name);
bool builtins_is_read_file(const char *name);
bool builtins_is_write_file(const char *name);
bool builtins_is_input(const char *name);

#endif
