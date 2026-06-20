#ifndef VIPER_SEMANTIC_H
#define VIPER_SEMANTIC_H

#include <stdbool.h>

#include "arena.h"
#include "ast.h"
#include "diagnostics.h"
#include "module.h"
#include "symbol_table.h"

typedef struct SemanticContext {
    SymbolTable *symtab;
    DiagContext *diag;
    Arena *arena;
    const char *file;
    const char *current_module;
    char **opened_modules;
    int open_count;
    ModuleIndex *module_index;
    ViperType current_return_type;
    bool in_function;
} SemanticContext;

bool semantic_check_program(SemanticContext *ctx, AstNode *program);
bool semantic_register_fn(SemanticContext *ctx, AstNode *fn_node);

#endif
