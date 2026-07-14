#include "lexer.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static bool token_list_push(TokenList *list, Token token) {
    if (list->count >= list->capacity) {
        int new_cap = list->capacity == 0 ? 64 : list->capacity * 2;
        Token *tokens = (Token *)realloc(list->tokens, (size_t)new_cap * sizeof(Token));
        if (!tokens) {
            return false;
        }
        list->tokens = tokens;
        list->capacity = new_cap;
    }
    list->tokens[list->count++] = token;
    return true;
}

void token_list_free(TokenList *list) {
    free(list->tokens);
    list->tokens = NULL;
    list->count = 0;
    list->capacity = 0;
}

void lexer_init(Lexer *lex, const char *source, int length, const char *file, DiagContext *diag) {
    lex->source = source;
    lex->file = file;
    lex->length = length;
    lex->pos = 0;
    lex->line = 1;
    lex->column = 1;
    lex->diag = diag;
}

static char lexer_peek(Lexer *lex, int offset) {
    int idx = lex->pos + offset;
    if (idx >= lex->length) {
        return '\0';
    }
    return lex->source[idx];
}

static char lexer_advance(Lexer *lex) {
    char ch = lexer_peek(lex, 0);
    if (ch == '\0') {
        return '\0';
    }
    lex->pos++;
    if (ch == '\n') {
        lex->line++;
        lex->column = 1;
    } else {
        lex->column++;
    }
    return ch;
}

static Token make_token(Lexer *lex, TokenKind kind, const char *start, int length) {
    Token t = {kind, start, length, lex->line, lex->column - (kind == TOK_NEWLINE ? 1 : length)};
    if (kind == TOK_NEWLINE) {
        t.line = lex->line - 1;
    }
    return t;
}

static bool is_identifier_start(char ch) {
    return isalpha((unsigned char)ch) || ch == '_';
}

static bool is_identifier_part(char ch) {
    return isalnum((unsigned char)ch) || ch == '_';
}

static TokenKind keyword_kind(const char *start, int length) {
    struct {
        const char *word;
        TokenKind kind;
    } keywords[] = {
        {"int", TOK_INT},       {"float", TOK_FLOAT},   {"string", TOK_STRING},
        {"bool", TOK_BOOL},     {"tensor", TOK_TENSOR}, {"list", TOK_LIST},
        {"struct", TOK_STRUCT}, {"fn", TOK_FN},
        {"if", TOK_IF},         {"else", TOK_ELSE},     {"for", TOK_FOR},
        {"while", TOK_WHILE},   {"in", TOK_IN},         {"return", TOK_RETURN},
        {"break", TOK_BREAK},   {"continue", TOK_CONTINUE},
        {"module", TOK_MODULE}, {"export", TOK_EXPORT}, {"open", TOK_OPEN},
        {"true", TOK_TRUE},     {"false", TOK_FALSE},
    };
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        if ((int)strlen(keywords[i].word) == length &&
            strncmp(start, keywords[i].word, (size_t)length) == 0) {
            return keywords[i].kind;
        }
    }
    return TOK_IDENTIFIER;
}

static bool lex_identifier(Lexer *lex, TokenList *out) {
    const char *start = lex->source + lex->pos;
    lexer_advance(lex);
    while (is_identifier_part(lexer_peek(lex, 0))) {
        lexer_advance(lex);
    }
    int length = (int)(lex->source + lex->pos - start);
    TokenKind kind = keyword_kind(start, length);
    return token_list_push(out, make_token(lex, kind, start, length));
}

static bool lex_number(Lexer *lex, TokenList *out) {
    const char *start = lex->source + lex->pos;
    TokenKind kind = TOK_INT_LITERAL;
    while (isdigit((unsigned char)lexer_peek(lex, 0))) {
        lexer_advance(lex);
    }
    if (lexer_peek(lex, 0) == '.' && isdigit((unsigned char)lexer_peek(lex, 1))) {
        kind = TOK_FLOAT_LITERAL;
        lexer_advance(lex);
        while (isdigit((unsigned char)lexer_peek(lex, 0))) {
            lexer_advance(lex);
        }
    }
    int length = (int)(lex->source + lex->pos - start);
    return token_list_push(out, make_token(lex, kind, start, length));
}

static bool lex_string(Lexer *lex, TokenList *out) {
    int start_line = lex->line;
    int start_col = lex->column;
    lexer_advance(lex);
    const char *start = lex->source + lex->pos;
    while (lexer_peek(lex, 0) != '"' && lexer_peek(lex, 0) != '\0') {
        if (lexer_peek(lex, 0) == '\n') {
            diag_emit(lex->diag, DIAG_ERROR, lex->file, start_line, start_col,
                      "unterminated string literal");
            return false;
        }
        if (lexer_peek(lex, 0) == '\\') {
            lexer_advance(lex);
            if (lexer_peek(lex, 0) != '\0') {
                lexer_advance(lex);
            }
            continue;
        }
        lexer_advance(lex);
    }
    if (lexer_peek(lex, 0) != '"') {
        diag_emit(lex->diag, DIAG_ERROR, lex->file, start_line, start_col,
                  "unterminated string literal");
        return false;
    }
    int length = (int)(lex->source + lex->pos - start);
    Token t = {TOK_STRING_LITERAL, start, length, start_line, start_col};
    token_list_push(out, t);
    lexer_advance(lex);
    return true;
}

static bool lex_operator(Lexer *lex, TokenList *out, char ch) {
    const char *start = lex->source + lex->pos;
    TokenKind kind;
    int length = 1;

    switch (ch) {
    case ':': kind = TOK_COLON; break;
    case '=': kind = (lexer_peek(lex, 1) == '=') ? (length = 2, TOK_EQEQ) : TOK_EQUAL; break;
    case ',': kind = TOK_COMMA; break;
    case '(': kind = TOK_LPAREN; break;
    case ')': kind = TOK_RPAREN; break;
    case '{': kind = TOK_LBRACE; break;
    case '}': kind = TOK_RBRACE; break;
    case '[': kind = TOK_LBRACKET; break;
    case ']': kind = TOK_RBRACKET; break;
    case '+': kind = TOK_PLUS; break;
    case '-':
        kind = (lexer_peek(lex, 1) == '>') ? (length = 2, TOK_ARROW) : TOK_MINUS;
        break;
    case '*': kind = TOK_STAR; break;
    case '/': kind = TOK_SLASH; break;
    case '@': kind = TOK_AT; break;
    case '.':
        if (lexer_peek(lex, 1) == '.') {
            kind = TOK_DOTDOT;
            length = 2;
        } else {
            kind = TOK_DOT;
        }
        break;
    case '!':
        if (lexer_peek(lex, 1) == '=') {
            kind = TOK_NEQ;
            length = 2;
        } else {
            kind = TOK_BANG;
        }
        break;
    case '&':
        if (lexer_peek(lex, 1) == '&') {
            kind = TOK_ANDAND;
            length = 2;
        } else {
            diag_emit(lex->diag, DIAG_ERROR, lex->file, lex->line, lex->column,
                      "unexpected character '&'");
            lexer_advance(lex);
            return false;
        }
        break;
    case '|':
        if (lexer_peek(lex, 1) == '|') {
            kind = TOK_OROR;
            length = 2;
        } else {
            diag_emit(lex->diag, DIAG_ERROR, lex->file, lex->line, lex->column,
                      "unexpected character '|'");
            lexer_advance(lex);
            return false;
        }
        break;
    case '<':
        kind = (lexer_peek(lex, 1) == '=') ? (length = 2, TOK_LTE) : TOK_LT;
        break;
    case '>':
        kind = (lexer_peek(lex, 1) == '=') ? (length = 2, TOK_GTE) : TOK_GT;
        break;
    default:
        diag_emit(lex->diag, DIAG_ERROR, lex->file, lex->line, lex->column,
                  "unexpected character '%c'", ch);
        lexer_advance(lex);
        return false;
    }

    for (int i = 0; i < length; i++) {
        lexer_advance(lex);
    }
    return token_list_push(out, make_token(lex, kind, start, length));
}

bool lexer_tokenize(Lexer *lex, TokenList *out) {
    out->tokens = NULL;
    out->count = 0;
    out->capacity = 0;

    while (lex->pos < lex->length) {
        char ch = lexer_peek(lex, 0);
        if (ch == ' ' || ch == '\t' || ch == '\r') {
            lexer_advance(lex);
            continue;
        }
        if (ch == '#') {
            while (lexer_peek(lex, 0) != '\n' && lexer_peek(lex, 0) != '\0') {
                lexer_advance(lex);
            }
            continue;
        }
        if (ch == '\n') {
            const char *start = lex->source + lex->pos;
            lexer_advance(lex);
            if (!token_list_push(out, make_token(lex, TOK_NEWLINE, start, 1))) {
                return false;
            }
            continue;
        }
        if (is_identifier_start(ch)) {
            if (!lex_identifier(lex, out)) {
                return false;
            }
            continue;
        }
        if (isdigit((unsigned char)ch)) {
            if (!lex_number(lex, out)) {
                return false;
            }
            continue;
        }
        if (ch == '"') {
            if (!lex_string(lex, out)) {
                return false;
            }
            continue;
        }
        if (!lex_operator(lex, out, ch)) {
            return false;
        }
    }

    Token eof = {TOK_EOF, lex->source + lex->pos, 0, lex->line, lex->column};
    return token_list_push(out, eof);
}
