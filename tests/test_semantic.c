#include "arena.h"
#include "diagnostics.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "symbol_table.h"
#include "types.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool compile_source(const char *source, const char *file, SymbolTable *symtab, DiagContext *diag) {
    Arena arena;
    assert(arena_init(&arena, 1024 * 64));

    Lexer lex;
    lexer_init(&lex, source, (int)strlen(source), file, diag);
    TokenList tokens;
    if (!lexer_tokenize(&lex, &tokens)) {
        arena_free(&arena);
        return false;
    }

    Parser parser;
    parser_init(&parser, &tokens, &arena, diag, file, "test");
    AstNode *program = parse_program(&parser);
    if (diag->error_count > 0) {
        token_list_free(&tokens);
        arena_free(&arena);
        return false;
    }

    SemanticContext sem = {symtab, diag, &arena, file, NULL, NULL, 0, NULL, viper_type_void(), false, 0};
    bool ok = semantic_check_program(&sem, program);
    token_list_free(&tokens);
    arena_free(&arena);
    return ok && diag->error_count == 0;
}

int main(void) {
    SymbolTable *st = symtab_create();
    DiagContext diag;
    assert(diag_init(&diag, 16));

    const char *good = "A: tensor[float, 2, 2] = [[1.0, 2.0], [3.0, 4.0]]\n"
                       "B: tensor[float, 2, 2] = [[1.0, 0.0], [0.0, 1.0]]\n"
                       "C: tensor[float, 2, 2] = A @ B\n";
    assert(compile_source(good, "good.vp", st, &diag));

    DiagContext bad_diag;
    assert(diag_init(&bad_diag, 16));
    const char *bad = "A: tensor[float, 2, 3] = [[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]\n"
                      "B: tensor[float, 2, 4] = [[1.0, 2.0, 3.0, 4.0], [5.0, 6.0, 7.0, 8.0]]\n"
                      "C: tensor[float, 2, 4] = A @ B\n";
    assert(!compile_source(bad, "bad.vp", st, &bad_diag));
    assert(bad_diag.error_count > 0);

    diag_free(&bad_diag);
    symtab_destroy(st);
    diag_free(&diag);
    printf("test_semantic: OK\n");
    return 0;
}
