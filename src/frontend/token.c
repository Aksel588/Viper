#include "token.h"

const char *token_kind_name(TokenKind kind) {
    switch (kind) {
    case TOK_EOF: return "EOF";
    case TOK_NEWLINE: return "NEWLINE";
    case TOK_IDENTIFIER: return "IDENTIFIER";
    case TOK_INT_LITERAL: return "INT_LITERAL";
    case TOK_FLOAT_LITERAL: return "FLOAT_LITERAL";
    case TOK_STRING_LITERAL: return "STRING_LITERAL";
    case TOK_INT: return "int";
    case TOK_FLOAT: return "float";
    case TOK_STRING: return "string";
    case TOK_BOOL: return "bool";
    case TOK_TENSOR: return "tensor";
    case TOK_FN: return "fn";
    case TOK_IF: return "if";
    case TOK_ELSE: return "else";
    case TOK_FOR: return "for";
    case TOK_WHILE: return "while";
    case TOK_IN: return "in";
    case TOK_RETURN: return "return";
    case TOK_MODULE: return "module";
    case TOK_EXPORT: return "export";
    case TOK_OPEN: return "open";
    case TOK_DOT: return ".";
    case TOK_TRUE: return "true";
    case TOK_FALSE: return "false";
    case TOK_COLON: return ":";
    case TOK_EQUAL: return "=";
    case TOK_COMMA: return ",";
    case TOK_LPAREN: return "(";
    case TOK_RPAREN: return ")";
    case TOK_LBRACE: return "{";
    case TOK_RBRACE: return "}";
    case TOK_LBRACKET: return "[";
    case TOK_RBRACKET: return "]";
    case TOK_ARROW: return "->";
    case TOK_DOTDOT: return "..";
    case TOK_PLUS: return "+";
    case TOK_MINUS: return "-";
    case TOK_STAR: return "*";
    case TOK_SLASH: return "/";
    case TOK_AT: return "@";
    case TOK_EQEQ: return "==";
    case TOK_NEQ: return "!=";
    case TOK_LT: return "<";
    case TOK_GT: return ">";
    case TOK_LTE: return "<=";
    case TOK_GTE: return ">=";
    default: return "UNKNOWN";
    }
}
