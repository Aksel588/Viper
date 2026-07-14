#include "arena.h"
#include "builtins.h"
#include "diagnostics.h"
#include "discovery.h"
#include "lexer.h"
#include "module.h"
#include "parser.h"
#include "semantic.h"
#include "source.h"
#include "symbol_table.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool compile_with_modules(const char *source, const char *file, const char *default_mod, SymbolTable *st,
                                 ModuleIndex *idx, DiagContext *diag) {
    Arena arena;
    if (!arena_init(&arena, 1024 * 64)) {
        return false;
    }

    Lexer lex;
    lexer_init(&lex, source, (int)strlen(source), file, diag);
    TokenList tokens;
    if (!lexer_tokenize(&lex, &tokens)) {
        arena_free(&arena);
        return false;
    }

    Parser parser;
    parser_init(&parser, &tokens, &arena, diag, file, default_mod);
    AstNode *program = parse_program(&parser);
    if (diag->error_count > 0 || !program) {
        token_list_free(&tokens);
        arena_free(&arena);
        return false;
    }

    SemanticContext sem = {st, diag, &arena, file, NULL, NULL, 0, idx, viper_type_void(), false, 0};
    bool ok = semantic_check_program(&sem, program);
    token_list_free(&tokens);
    arena_free(&arena);
    return ok && diag->error_count == 0;
}

int main(void) {
    ModuleIndex index;
    DiagContext diag;
    assert(diag_init(&diag, 16));
    assert(module_index_build("examples", true, &index, &diag));

    SymbolTable *st = symtab_create();
    builtins_register(st);

    const char *math_src = "module math\nexport fn add(a: int, b: int) -> int { return a + b }\n";
    DiagContext math_diag;
    assert(diag_init(&math_diag, 16));
    assert(compile_with_modules(math_src, "examples/math.vp", "math", st, &index, &math_diag));

    SemanticContext reg = {st, &math_diag, NULL, "examples/math.vp", NULL, NULL, 0, &index, viper_type_void(), false, 0};
    Arena reg_arena;
    assert(arena_init(&reg_arena, 4096));
    reg.arena = &reg_arena;
    Lexer lex;
    lexer_init(&lex, math_src, (int)strlen(math_src), "examples/math.vp", &math_diag);
    TokenList tokens;
    assert(lexer_tokenize(&lex, &tokens));
    Parser parser;
    parser_init(&parser, &tokens, &reg_arena, &math_diag, "examples/math.vp", "math");
    AstNode **fns = NULL;
    int fn_count = 0;
    assert(parse_fn_signatures_only(&parser, &fns, &fn_count));
    assert(fn_count == 1);
    assert(semantic_register_fn(&reg, fns[0]));
    assert(symtab_lookup_qualified(st, "math", "add") != NULL);
    free(fns);
    token_list_free(&tokens);
    arena_free(&reg_arena);

    const char *globals_src = "open math\nresult: int = add(1, 2)\n";
    DiagContext globals_diag;
    assert(diag_init(&globals_diag, 16));
    assert(compile_with_modules(globals_src, "examples/globals.vp", "globals", st, &index, &globals_diag));

    const char *bad_src = "result: int = add(1, 2)\n";
    DiagContext bad_diag;
    assert(diag_init(&bad_diag, 16));
    assert(!compile_with_modules(bad_src, "examples/bad.vp", "bad", st, &index, &bad_diag));
    assert(bad_diag.error_count > 0);

    diag_free(&bad_diag);
    diag_free(&globals_diag);
    diag_free(&math_diag);
    symtab_destroy(st);
    module_index_free(&index);
    diag_free(&diag);
    printf("test_module: OK\n");
    return 0;
}
