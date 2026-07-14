#include "bytecode.h"
#include "diagnostics.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "symbol_table.h"
#include "builtins.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool compile_source(const char *source, BcProgram *out) {
    DiagContext diag;
    assert(diag_init(&diag, 16));
    Arena arena;
    assert(arena_init(&arena, 1024 * 64));

    Lexer lex;
    lexer_init(&lex, source, (int)strlen(source), "test.vp", &diag);
    TokenList tokens;
    assert(lexer_tokenize(&lex, &tokens));

    Parser parser;
    parser_init(&parser, &tokens, &arena, &diag, "test.vp", "test");
    AstNode *program = parse_program(&parser);
    assert(program != NULL);

    SymbolTable *st = symtab_create();
    builtins_register(st);
    SemanticContext sem = {st, &diag, &arena, "test.vp", NULL, NULL, 0, NULL, viper_type_void(), false, 0};
    assert(semantic_check_program(&sem, program));
    assert(bc_compile_program(program, NULL, 0, out, &diag));

    symtab_destroy(st);
    token_list_free(&tokens);
    arena_free(&arena);
    diag_free(&diag);
    return true;
}

int main(void) {
    BcProgram prog;
    compile_source("x: int = 1 + 2\nprint(x)\n", &prog);
    assert(prog.code_count > 0);
    assert(prog.code[prog.code_count - 1].op == BC_HALT);
    bc_program_free(&prog);

    compile_source("x: float = float(42)\n", &prog);
    bc_program_free(&prog);

    printf("test_bytecode: OK\n");
    return 0;
}
