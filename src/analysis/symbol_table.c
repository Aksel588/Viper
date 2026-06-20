#include "symbol_table.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint32_t fnv1a_hash(const char *str) {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (unsigned char)*str++;
        hash *= 16777619u;
    }
    return hash;
}

static SymMap *symmap_get_current(SymbolTable *st) {
    if (st->scope_count > 0) {
        return st->scopes[st->scope_count - 1];
    }
    return &st->global;
}

static Symbol *symmap_lookup_in(SymMap *map, const char *name) {
    uint32_t idx = fnv1a_hash(name) % SYMTAB_BUCKET_COUNT;
    for (Symbol *s = map->buckets[idx]; s; s = s->next) {
        if (strcmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

static Symbol *symmap_insert(SymMap *map, Symbol *sym) {
    uint32_t idx = fnv1a_hash(sym->name) % SYMTAB_BUCKET_COUNT;
    if (symmap_lookup_in(map, sym->name)) {
        return NULL;
    }
    sym->next = map->buckets[idx];
    map->buckets[idx] = sym;
    return sym;
}

static void symbol_free(Symbol *s) {
    free(s->name);
    free(s->module_name);
    free(s->simple_name);
    free(s->param_types);
    free(s->param_names);
    free(s);
}

SymbolTable *symtab_create(void) {
    SymbolTable *st = (SymbolTable *)calloc(1, sizeof(SymbolTable));
    if (!st) {
        return NULL;
    }
    st->scope_capacity = 8;
    st->scopes = (SymMap **)calloc((size_t)st->scope_capacity, sizeof(SymMap *));
    if (!st->scopes) {
        free(st);
        return NULL;
    }
    return st;
}

void symtab_destroy(SymbolTable *st) {
    if (!st) {
        return;
    }

    for (int i = 0; i < SYMTAB_BUCKET_COUNT; i++) {
        Symbol *s = st->global.buckets[i];
        while (s) {
            Symbol *next = s->next;
            symbol_free(s);
            s = next;
        }
    }

    for (int i = 0; i < st->scope_count; i++) {
        SymMap *map = st->scopes[i];
        if (!map) {
            continue;
        }
        for (int b = 0; b < SYMTAB_BUCKET_COUNT; b++) {
            Symbol *s = map->buckets[b];
            while (s) {
                Symbol *next = s->next;
                symbol_free(s);
                s = next;
            }
        }
        free(map);
    }

    free(st->scopes);
    free(st);
}

void symtab_push_scope(SymbolTable *st) {
    if (st->scope_count >= st->scope_capacity) {
        int new_cap = st->scope_capacity * 2;
        SymMap **scopes = (SymMap **)realloc(st->scopes, (size_t)new_cap * sizeof(SymMap *));
        if (!scopes) {
            return;
        }
        st->scopes = scopes;
        st->scope_capacity = new_cap;
    }
    SymMap *map = (SymMap *)calloc(1, sizeof(SymMap));
    if (!map) {
        return;
    }
    st->scopes[st->scope_count++] = map;
}

void symtab_pop_scope(SymbolTable *st) {
    if (st->scope_count == 0) {
        return;
    }
    SymMap *map = st->scopes[--st->scope_count];
    for (int b = 0; b < SYMTAB_BUCKET_COUNT; b++) {
        Symbol *s = map->buckets[b];
        while (s) {
            Symbol *next = s->next;
            symbol_free(s);
            s = next;
        }
    }
    free(map);
    st->scopes[st->scope_count] = NULL;
}

void symtab_make_qualified_key(const char *module, const char *name, char *buf, size_t len) {
    snprintf(buf, len, "%s.%s", module, name);
}

Symbol *symtab_lookup_qualified(SymbolTable *st, const char *module, const char *name) {
    char key[256];
    symtab_make_qualified_key(module, name, key, sizeof(key));
    return symmap_lookup_in(&st->global, key);
}

Symbol *symtab_register_exported(SymbolTable *st, const char *module, Symbol *sym) {
    char key[256];
    const char *simple = sym->simple_name ? sym->simple_name : sym->name;
    symtab_make_qualified_key(module, simple, key, sizeof(key));
    free(sym->name);
    sym->name = strdup(key);
    sym->module_name = strdup(module);
    if (!sym->simple_name) {
        sym->simple_name = strdup(simple);
    }
    sym->exported = true;
    return symmap_insert(&st->global, sym);
}

Symbol *symtab_lookup_with_opens(SymbolTable *st, const char *name, const char **opens, int open_count) {
    for (int i = 0; i < open_count; i++) {
        Symbol *s = symtab_lookup_qualified(st, opens[i], name);
        if (s) {
            return s;
        }
    }
    return NULL;
}

Symbol *symtab_insert(SymbolTable *st, Symbol *sym) {
    return symmap_insert(symmap_get_current(st), sym);
}

Symbol *symtab_lookup(SymbolTable *st, const char *name) {
    for (int i = st->scope_count - 1; i >= 0; i--) {
        Symbol *s = symmap_lookup_in(st->scopes[i], name);
        if (s) {
            return s;
        }
    }
    return symmap_lookup_in(&st->global, name);
}

Symbol *symtab_lookup_global(SymbolTable *st, const char *name) {
    return symmap_lookup_in(&st->global, name);
}

Symbol *symtab_register_global(SymbolTable *st, Symbol *sym) {
    return symmap_insert(&st->global, sym);
}

Symbol *symbol_create(const char *name, SymbolKind kind, ViperType type, int line, int column) {
    Symbol *sym = (Symbol *)calloc(1, sizeof(Symbol));
    if (!sym) {
        return NULL;
    }
    sym->name = strdup(name);
    sym->simple_name = strdup(name);
    if (!sym->name || !sym->simple_name) {
        free(sym->name);
        free(sym->simple_name);
        free(sym);
        return NULL;
    }
    sym->kind = kind;
    sym->type = type;
    sym->line = line;
    sym->column = column;
    return sym;
}
