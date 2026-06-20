#ifndef VIPER_BUILTINS_H
#define VIPER_BUILTINS_H

#include "symbol_table.h"

void builtins_register(SymbolTable *st);
bool builtins_is(const char *name);
bool builtins_is_transpose(const char *name);
bool builtins_is_reshape(const char *name);

#endif
