#ifndef VIPER_PARSER_H
#define VIPER_PARSER_H

#include <stdbool.h>

#include "ast.h"
#include "diagnostics.h"
#include "lexer.h"
#include "module.h"

typedef struct Parser {
    TokenList *tokens;
    int pos;
    Arena *arena;
    DiagContext *diag;
    const char *file;
    const char *default_module;
    const char *current_module;
} Parser;

void parser_init(Parser *p, TokenList *tokens, Arena *arena, DiagContext *diag, const char *file,
                 const char *default_module);
bool parse_file_header(Parser *p, FileHeader *out);
AstNode *parse_program(Parser *p);
bool parse_fn_signatures_only(Parser *p, AstNode ***out_nodes, int *out_count);

#endif
