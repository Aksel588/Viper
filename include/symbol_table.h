#ifndef VIPER_SYMBOL_TABLE_H
#define VIPER_SYMBOL_TABLE_H

#include <stdbool.h>

#include "types.h"

typedef enum {
    SYM_VAR,
    SYM_FN,
    SYM_STRUCT
} SymbolKind;

typedef struct Symbol {
    char *name;
    char *module_name;
    char *simple_name;
    SymbolKind kind;
    ViperType type;
    ViperType *param_types;
    char **param_names;
    int param_count;
    int line;
    int column;
    bool exported;
    struct Symbol *next;
} Symbol;

#define SYMTAB_BUCKET_COUNT 256

typedef struct SymMap {
    Symbol *buckets[SYMTAB_BUCKET_COUNT];
} SymMap;

typedef struct SymbolTable {
    SymMap global;
    SymMap **scopes;
    int scope_count;
    int scope_capacity;
} SymbolTable;

SymbolTable *symtab_create(void);
void symtab_destroy(SymbolTable *st);

void symtab_push_scope(SymbolTable *st);
void symtab_pop_scope(SymbolTable *st);

Symbol *symtab_insert(SymbolTable *st, Symbol *sym);
Symbol *symtab_lookup(SymbolTable *st, const char *name);
Symbol *symtab_lookup_global(SymbolTable *st, const char *name);
Symbol *symtab_register_global(SymbolTable *st, Symbol *sym);

void symtab_make_qualified_key(const char *module, const char *name, char *buf, size_t len);
Symbol *symtab_lookup_qualified(SymbolTable *st, const char *module, const char *name);
Symbol *symtab_register_exported(SymbolTable *st, const char *module, Symbol *sym);
Symbol *symtab_lookup_with_opens(SymbolTable *st, const char *name, const char **opens, int open_count);
Symbol *symtab_lookup_struct(SymbolTable *st, const char *name);

Symbol *symbol_create(const char *name, SymbolKind kind, ViperType type, int line, int column);

#endif
