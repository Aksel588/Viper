#include "symbol_table.h"
#include "types.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    SymbolTable *st = symtab_create();
    assert(st != NULL);

    Symbol *a = symbol_create("alpha", SYM_VAR, viper_type_int(), 1, 1);
    Symbol *b = symbol_create("beta", SYM_VAR, viper_type_float(), 2, 1);
    assert(symtab_register_global(st, a) == a);
    assert(symtab_register_global(st, b) == b);

    Symbol *found = symtab_lookup_global(st, "alpha");
    assert(found != NULL);
    assert(found->type.kind == TYPE_INT);

    Symbol *dup = symbol_create("alpha", SYM_VAR, viper_type_int(), 3, 1);
    assert(symtab_register_global(st, dup) == NULL);
    free(dup->name);
    free(dup);

    symtab_push_scope(st);
    Symbol *local = symbol_create("gamma", SYM_VAR, viper_type_bool(), 4, 1);
    assert(symtab_insert(st, local) == local);
    assert(symtab_lookup(st, "gamma") == local);
    assert(symtab_lookup(st, "alpha") == a);
    symtab_pop_scope(st);
    assert(symtab_lookup(st, "gamma") == NULL);

    symtab_destroy(st);
    printf("test_symbol_table: OK\n");
    return 0;
}
