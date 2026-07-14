#ifndef VIPER_TOKEN_H
#define VIPER_TOKEN_H

typedef enum {
    TOK_EOF,
    TOK_NEWLINE,
    TOK_IDENTIFIER,
    TOK_INT_LITERAL,
    TOK_FLOAT_LITERAL,
    TOK_STRING_LITERAL,
    TOK_INT,
    TOK_FLOAT,
    TOK_STRING,
    TOK_BOOL,
    TOK_TENSOR,
    TOK_LIST,
    TOK_STRUCT,
    TOK_FN,
    TOK_IF,
    TOK_ELSE,
    TOK_FOR,
    TOK_WHILE,
    TOK_IN,
    TOK_RETURN,
    TOK_BREAK,
    TOK_CONTINUE,
    TOK_MODULE,
    TOK_EXPORT,
    TOK_OPEN,
    TOK_DOT,
    TOK_TRUE,
    TOK_FALSE,
    TOK_COLON,
    TOK_EQUAL,
    TOK_COMMA,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_ARROW,
    TOK_DOTDOT,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_AT,
    TOK_BANG,
    TOK_ANDAND,
    TOK_OROR,
    TOK_EQEQ,
    TOK_NEQ,
    TOK_LT,
    TOK_GT,
    TOK_LTE,
    TOK_GTE
} TokenKind;

typedef struct Token {
    TokenKind kind;
    const char *start;
    int length;
    int line;
    int column;
} Token;

const char *token_kind_name(TokenKind kind);

#endif
