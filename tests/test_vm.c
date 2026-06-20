#include "bytecode.h"
#include "diagnostics.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "symbol_table.h"
#include "builtins.h"
#include "vm.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool run_source(const char *source, DiagContext *diag) {
    Arena arena;
    assert(arena_init(&arena, 1024 * 64));

    Lexer lex;
    lexer_init(&lex, source, (int)strlen(source), "test.vp", diag);
    TokenList tokens;
    if (!lexer_tokenize(&lex, &tokens)) {
        arena_free(&arena);
        return false;
    }

    Parser parser;
    parser_init(&parser, &tokens, &arena, diag, "test.vp", "test");
    AstNode *program = parse_program(&parser);
    if (diag->error_count > 0 || !program) {
        token_list_free(&tokens);
        arena_free(&arena);
        return false;
    }

    SymbolTable *st = symtab_create();
    builtins_register(st);
    SemanticContext sem = {st, diag, &arena, "test.vp", NULL, NULL, 0, NULL, viper_type_void(), false};
    if (!semantic_check_program(&sem, program)) {
        symtab_destroy(st);
        token_list_free(&tokens);
        arena_free(&arena);
        return false;
    }

    BcProgram prog;
    if (!bc_compile_program(program, NULL, 0, &prog, diag)) {
        symtab_destroy(st);
        token_list_free(&tokens);
        arena_free(&arena);
        return false;
    }

    bool ok = vm_run(&prog, diag);
    bc_program_free(&prog);
    symtab_destroy(st);
    token_list_free(&tokens);
    arena_free(&arena);
    return ok;
}

int main(void) {
    DiagContext diag;
    assert(diag_init(&diag, 16));

    const char *matmul =
        "A: tensor[float, 2, 2] = [[1.0, 2.0], [3.0, 4.0]]\n"
        "B: tensor[float, 2, 2] = [[1.0, 0.0], [0.0, 1.0]]\n"
        "C: tensor[float, 2, 2] = A @ B\n";
    assert(run_source(matmul, &diag));

    DiagContext diag2;
    assert(diag_init(&diag2, 16));
    const char *ops =
        "A: tensor[float, 2, 2] = [[1.0, 2.0], [3.0, 4.0]]\n"
        "B: tensor[float, 2, 2] = [[1.0, 1.0], [1.0, 1.0]]\n"
        "C: tensor[float, 2, 2] = A + B\n"
        "T: tensor[float, 2, 2] = transpose(A)\n";
    assert(run_source(ops, &diag2));

    diag_free(&diag2);
    diag_free(&diag);
    printf("test_vm: OK\n");
    return 0;
}
