#include "diagnostics.h"
#include "lexer.h"
#include "token.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *source = "x: int = 42\nA @ B\nfor i in 0..10 { }\n!true && false || true\n";
    DiagContext diag;
    assert(diag_init(&diag, 8));

    Lexer lex;
    lexer_init(&lex, source, (int)strlen(source), "test.vp", &diag);
    TokenList tokens;
    assert(lexer_tokenize(&lex, &tokens));
    assert(diag.error_count == 0);

    int at_count = 0;
    int dotdot_count = 0;
    int bang_count = 0;
    int andand_count = 0;
    int oror_count = 0;
    for (int i = 0; i < tokens.count; i++) {
        if (tokens.tokens[i].kind == TOK_AT) {
            at_count++;
        }
        if (tokens.tokens[i].kind == TOK_DOTDOT) {
            dotdot_count++;
        }
        if (tokens.tokens[i].kind == TOK_BANG) {
            bang_count++;
        }
        if (tokens.tokens[i].kind == TOK_ANDAND) {
            andand_count++;
        }
        if (tokens.tokens[i].kind == TOK_OROR) {
            oror_count++;
        }
    }
    assert(at_count == 1);
    assert(dotdot_count == 1);
    assert(bang_count == 1);
    assert(andand_count == 1);
    assert(oror_count == 1);
    assert(tokens.tokens[tokens.count - 1].kind == TOK_EOF);

    token_list_free(&tokens);
    diag_free(&diag);
    printf("test_lexer: OK\n");
    return 0;
}
