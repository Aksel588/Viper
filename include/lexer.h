#ifndef VIPER_LEXER_H
#define VIPER_LEXER_H

#include <stdbool.h>

#include "diagnostics.h"
#include "token.h"

typedef struct Lexer {
    const char *source;
    const char *file;
    int length;
    int pos;
    int line;
    int column;
    DiagContext *diag;
} Lexer;

typedef struct TokenList {
    Token *tokens;
    int count;
    int capacity;
} TokenList;

void lexer_init(Lexer *lex, const char *source, int length, const char *file, DiagContext *diag);
bool lexer_tokenize(Lexer *lex, TokenList *out);
void token_list_free(TokenList *list);

#endif
