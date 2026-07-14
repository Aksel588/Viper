#include "parser.h"

#include <stdlib.h>
#include <string.h>

static Token *parser_current(Parser *p) {
    return &p->tokens->tokens[p->pos];
}

static Token *parser_peek(Parser *p, int offset) {
    int idx = p->pos + offset;
    if (idx >= p->tokens->count) {
        return &p->tokens->tokens[p->tokens->count - 1];
    }
    return &p->tokens->tokens[idx];
}

static bool parser_check(Parser *p, TokenKind kind) {
    return parser_current(p)->kind == kind;
}

static bool parser_match(Parser *p, TokenKind kind) {
    if (parser_check(p, kind)) {
        p->pos++;
        return true;
    }
    return false;
}

static Token *parser_advance(Parser *p) {
    Token *t = parser_current(p);
    if (!parser_check(p, TOK_EOF)) {
        p->pos++;
    }
    return t;
}

static void parser_error(Parser *p, Token *tok, const char *msg) {
    diag_emit(p->diag, DIAG_ERROR, p->file, tok->line, tok->column, "%s", msg);
}

static Token *parser_consume(Parser *p, TokenKind kind, const char *msg) {
    if (parser_check(p, kind)) {
        return parser_advance(p);
    }
    parser_error(p, parser_current(p), msg);
    return NULL;
}

/* Allow type keywords as module names (e.g. `module string`). */
static bool is_name_token(TokenKind kind) {
    return kind == TOK_IDENTIFIER || kind == TOK_INT || kind == TOK_FLOAT || kind == TOK_STRING ||
           kind == TOK_BOOL || kind == TOK_TENSOR || kind == TOK_LIST || kind == TOK_STRUCT;
}

static Token *parser_consume_name(Parser *p, const char *msg) {
    if (is_name_token(parser_current(p)->kind)) {
        return parser_advance(p);
    }
    parser_error(p, parser_current(p), msg);
    return NULL;
}

static void skip_newlines(Parser *p) {
    while (parser_match(p, TOK_NEWLINE)) {
    }
}

static char *token_text(Arena *arena, Token *tok) {
    char *buf = (char *)arena_alloc(arena, (size_t)tok->length + 1);
    if (!buf) {
        return NULL;
    }
    memcpy(buf, tok->start, (size_t)tok->length);
    buf[tok->length] = '\0';
    return buf;
}

static ViperType parse_type(Parser *p);

static AstNode *parse_expression(Parser *p);
static AstNode *parse_statement(Parser *p);
static AstNode *parse_block(Parser *p);

static ViperType token_to_primitive_type(TokenKind kind) {
    switch (kind) {
    case TOK_INT: return viper_type_int();
    case TOK_FLOAT: return viper_type_float();
    case TOK_STRING: return viper_type_string();
    case TOK_BOOL: return viper_type_bool();
    default: return viper_type_unknown();
    }
}

static ViperType parse_type(Parser *p) {
    Token *tok = parser_current(p);
    if (tok->kind == TOK_LIST) {
        parser_advance(p);
        parser_consume(p, TOK_LBRACKET, "expected '[' after list");
        Token *elem_tok = parser_current(p);
        TypeKind elem = TYPE_UNKNOWN;
        if (elem_tok->kind == TOK_INT) {
            elem = TYPE_INT;
        } else if (elem_tok->kind == TOK_FLOAT) {
            elem = TYPE_FLOAT;
        } else if (elem_tok->kind == TOK_STRING) {
            elem = TYPE_STRING;
        } else if (elem_tok->kind == TOK_BOOL) {
            elem = TYPE_BOOL;
        } else {
            parser_error(p, elem_tok, "list element type must be int, float, string, or bool");
            return viper_type_unknown();
        }
        parser_advance(p);
        parser_consume(p, TOK_RBRACKET, "expected ']' after list element type");
        return viper_type_list(elem);
    }
    if (tok->kind == TOK_TENSOR) {
        parser_advance(p);
        parser_consume(p, TOK_LBRACKET, "expected '[' after tensor");
        Token *elem_tok = parser_current(p);
        TypeKind elem = TYPE_UNKNOWN;
        if (elem_tok->kind == TOK_INT) {
            elem = TYPE_INT;
        } else if (elem_tok->kind == TOK_FLOAT) {
            elem = TYPE_FLOAT;
        } else {
            parser_error(p, elem_tok, "tensor element type must be int or float");
            return viper_type_unknown();
        }
        parser_advance(p);

        int dims[VIPER_TENSOR_MAX_RANK];
        int rank = 0;
        while (parser_match(p, TOK_COMMA)) {
            Token *dim_tok = parser_consume(p, TOK_INT_LITERAL, "expected dimension literal");
            if (!dim_tok) {
                return viper_type_unknown();
            }
            char buf[32];
            memcpy(buf, dim_tok->start, (size_t)dim_tok->length);
            buf[dim_tok->length] = '\0';
            dims[rank++] = atoi(buf);
        }
        parser_consume(p, TOK_RBRACKET, "expected ']' after tensor dimensions");
        return viper_type_tensor(elem, dims, rank);
    }

    if (tok->kind == TOK_IDENTIFIER) {
        char *name = token_text(p->arena, tok);
        parser_advance(p);
        ViperType t = viper_type_struct(name);
        return t;
    }

    ViperType t = token_to_primitive_type(tok->kind);
    if (t.kind == TYPE_UNKNOWN) {
        parser_error(p, tok, "expected type");
        return viper_type_unknown();
    }
    parser_advance(p);
    return t;
}

static AstNode *parse_postfix(Parser *p);

static AstNode *parse_primary(Parser *p) {
    Token *tok = parser_current(p);

    if (tok->kind == TOK_INT_LITERAL) {
        parser_advance(p);
        char buf[64];
        memcpy(buf, tok->start, (size_t)tok->length);
        buf[tok->length] = '\0';
        return ast_literal_int(p->arena, tok->line, tok->column, atoll(buf));
    }
    if (tok->kind == TOK_FLOAT_LITERAL) {
        parser_advance(p);
        char buf[64];
        memcpy(buf, tok->start, (size_t)tok->length);
        buf[tok->length] = '\0';
        return ast_literal_float(p->arena, tok->line, tok->column, atof(buf));
    }
    if (tok->kind == TOK_STRING_LITERAL) {
        parser_advance(p);
        char *text = token_text(p->arena, tok);
        return ast_literal_string(p->arena, tok->line, tok->column, text);
    }
    if (tok->kind == TOK_TRUE || tok->kind == TOK_FALSE) {
        parser_advance(p);
        return ast_literal_bool(p->arena, tok->line, tok->column, tok->kind == TOK_TRUE);
    }
    if (tok->kind == TOK_IDENTIFIER) {
        int line = tok->line;
        int col = tok->column;
        parser_advance(p);
        char *first = token_text(p->arena, tok);
        if (parser_match(p, TOK_DOT) && parser_check(p, TOK_IDENTIFIER)) {
            Token *name_tok = parser_advance(p);
            char *second = token_text(p->arena, name_tok);
            if (parser_check(p, TOK_LPAREN)) {
                parser_advance(p);
                AstNode **args = NULL;
                int arg_count = 0;
                int arg_cap = 0;
                if (!parser_check(p, TOK_RPAREN)) {
                    do {
                        if (arg_count >= arg_cap) {
                            arg_cap = arg_cap == 0 ? 4 : arg_cap * 2;
                            args = (AstNode **)realloc(args, (size_t)arg_cap * sizeof(AstNode *));
                        }
                        args[arg_count++] = parse_expression(p);
                    } while (parser_match(p, TOK_COMMA));
                }
                parser_consume(p, TOK_RPAREN, "expected ')' after arguments");
                AstNode *call = ast_call(p->arena, line, col, first, second, args, arg_count);
                free(args);
                return call;
            }
            return ast_field_get(p->arena, line, col, ast_identifier(p->arena, line, col, first), second);
        }
        if (parser_check(p, TOK_LBRACE)) {
            parser_advance(p);
            char **field_names = NULL;
            AstNode **values = NULL;
            int field_count = 0;
            int field_cap = 0;
            if (!parser_check(p, TOK_RBRACE)) {
                do {
                    Token *field_tok = parser_consume(p, TOK_IDENTIFIER, "expected field name");
                    if (!field_tok) {
                        break;
                    }
                    parser_consume(p, TOK_COLON, "expected ':' after field name");
                    AstNode *val = parse_expression(p);
                    if (field_count >= field_cap) {
                        field_cap = field_cap == 0 ? 4 : field_cap * 2;
                        field_names = (char **)realloc(field_names, (size_t)field_cap * sizeof(char *));
                        values = (AstNode **)realloc(values, (size_t)field_cap * sizeof(AstNode *));
                    }
                    field_names[field_count] = token_text(p->arena, field_tok);
                    values[field_count++] = val;
                } while (parser_match(p, TOK_COMMA));
            }
            parser_consume(p, TOK_RBRACE, "expected '}' after struct literal");
            AstNode *lit = ast_struct_lit(p->arena, line, col, first, field_names, values, field_count);
            free(field_names);
            free(values);
            return lit;
        }
        if (parser_check(p, TOK_LPAREN)) {
            parser_advance(p);
            AstNode **args = NULL;
            int arg_count = 0;
            int arg_cap = 0;
            if (!parser_check(p, TOK_RPAREN)) {
                do {
                    if (arg_count >= arg_cap) {
                        arg_cap = arg_cap == 0 ? 4 : arg_cap * 2;
                        args = (AstNode **)realloc(args, (size_t)arg_cap * sizeof(AstNode *));
                    }
                    args[arg_count++] = parse_expression(p);
                } while (parser_match(p, TOK_COMMA));
            }
            parser_consume(p, TOK_RPAREN, "expected ')' after arguments");
            AstNode *call = ast_call(p->arena, line, col, NULL, first, args, arg_count);
            free(args);
            return call;
        }
        return ast_identifier(p->arena, line, col, first);
    }
    if (tok->kind == TOK_LBRACKET) {
        int line = tok->line;
        int col = tok->column;
        parser_advance(p);
        if (parser_check(p, TOK_LBRACKET)) {
            AstNode **rows = NULL;
            int row_count = 0;
            int row_cap = 0;
            if (!parser_check(p, TOK_RBRACKET)) {
                do {
                    parser_consume(p, TOK_LBRACKET, "expected '[' to start tensor row");
                    AstNode **elems = NULL;
                    int elem_count = 0;
                    int elem_cap = 0;
                    if (!parser_check(p, TOK_RBRACKET)) {
                        do {
                            if (elem_count >= elem_cap) {
                                elem_cap = elem_cap == 0 ? 4 : elem_cap * 2;
                                elems = (AstNode **)realloc(elems, (size_t)elem_cap * sizeof(AstNode *));
                            }
                            elems[elem_count++] = parse_expression(p);
                        } while (parser_match(p, TOK_COMMA));
                    }
                    parser_consume(p, TOK_RBRACKET, "expected ']' to end tensor row");
                    AstNode *row = ast_block(p->arena, line, col, elems, elem_count);
                    free(elems);
                    if (row_count >= row_cap) {
                        row_cap = row_cap == 0 ? 4 : row_cap * 2;
                        rows = (AstNode **)realloc(rows, (size_t)row_cap * sizeof(AstNode *));
                    }
                    rows[row_count++] = row;
                } while (parser_match(p, TOK_COMMA));
            }
            parser_consume(p, TOK_RBRACKET, "expected ']' to end tensor literal");
            AstNode *tensor = ast_literal_tensor(p->arena, line, col, rows, row_count);
            free(rows);
            return tensor;
        }
        AstNode **elements = NULL;
        int elem_count = 0;
        int elem_cap = 0;
        if (!parser_check(p, TOK_RBRACKET)) {
            do {
                if (elem_count >= elem_cap) {
                    elem_cap = elem_cap == 0 ? 4 : elem_cap * 2;
                    elements = (AstNode **)realloc(elements, (size_t)elem_cap * sizeof(AstNode *));
                }
                elements[elem_count++] = parse_expression(p);
            } while (parser_match(p, TOK_COMMA));
        }
        parser_consume(p, TOK_RBRACKET, "expected ']' to end list literal");
        AstNode *list = ast_literal_list(p->arena, line, col, elements, elem_count);
        free(elements);
        return list;
    }
    if (tok->kind == TOK_LPAREN) {
        parser_advance(p);
        AstNode *expr = parse_expression(p);
        parser_consume(p, TOK_RPAREN, "expected ')' after expression");
        return expr;
    }

    parser_error(p, tok, "expected expression");
    return NULL;
}

static AstNode *parse_postfix(Parser *p) {
    AstNode *expr = parse_primary(p);
    while (expr && parser_check(p, TOK_LBRACKET)) {
        int line = parser_current(p)->line;
        int col = parser_current(p)->column;
        parser_advance(p);
        AstNode **indices = NULL;
        int index_count = 0;
        int index_cap = 0;
        if (!parser_check(p, TOK_RBRACKET)) {
            do {
                if (index_count >= index_cap) {
                    index_cap = index_cap == 0 ? 4 : index_cap * 2;
                    indices = (AstNode **)realloc(indices, (size_t)index_cap * sizeof(AstNode *));
                }
                indices[index_count++] = parse_expression(p);
            } while (parser_match(p, TOK_COMMA));
        }
        parser_consume(p, TOK_RBRACKET, "expected ']' after index");
        expr = ast_index(p->arena, line, col, expr, indices, index_count);
        free(indices);
    }
    return expr;
}

static AstNode *parse_unary(Parser *p) {
    Token *tok = parser_current(p);
    if (tok->kind == TOK_INT || tok->kind == TOK_FLOAT || tok->kind == TOK_BOOL) {
        if (parser_peek(p, 1)->kind == TOK_LPAREN) {
            ViperType target = token_to_primitive_type(tok->kind);
            int line = tok->line;
            int col = tok->column;
            parser_advance(p);
            parser_consume(p, TOK_LPAREN, "expected '(' after cast type");
            AstNode *operand = parse_expression(p);
            parser_consume(p, TOK_RPAREN, "expected ')' after cast expression");
            return ast_cast(p->arena, line, col, target, operand);
        }
    }
    if (parser_match(p, TOK_MINUS)) {
        Token *minus = parser_peek(p, -1);
        AstNode *operand = parse_unary(p);
        return ast_unary(p->arena, minus->line, minus->column, OP_SUB, operand);
    }
    if (parser_match(p, TOK_BANG)) {
        Token *bang = parser_peek(p, -1);
        AstNode *operand = parse_unary(p);
        return ast_unary(p->arena, bang->line, bang->column, OP_NOT, operand);
    }
    return parse_postfix(p);
}

static BinOp token_to_binop(TokenKind kind) {
    switch (kind) {
    case TOK_PLUS: return OP_ADD;
    case TOK_MINUS: return OP_SUB;
    case TOK_STAR: return OP_MUL;
    case TOK_SLASH: return OP_DIV;
    case TOK_AT: return OP_MATMUL;
    case TOK_EQEQ: return OP_EQ;
    case TOK_NEQ: return OP_NEQ;
    case TOK_LT: return OP_LT;
    case TOK_GT: return OP_GT;
    case TOK_LTE: return OP_LTE;
    case TOK_GTE: return OP_GTE;
    default: return OP_ADD;
    }
}

static AstNode *parse_matmul(Parser *p) {
    AstNode *left = parse_unary(p);
    while (parser_check(p, TOK_AT)) {
        Token *tok = parser_advance(p);
        AstNode *right = parse_unary(p);
        left = ast_binary(p->arena, tok->line, tok->column, OP_MATMUL, left, right);
    }
    return left;
}

static AstNode *parse_comparison(Parser *p) {
    AstNode *left = parse_matmul(p);
    while (parser_check(p, TOK_EQEQ) || parser_check(p, TOK_NEQ) || parser_check(p, TOK_LT) ||
           parser_check(p, TOK_GT) || parser_check(p, TOK_LTE) || parser_check(p, TOK_GTE)) {
        Token *tok = parser_advance(p);
        AstNode *right = parse_matmul(p);
        left = ast_binary(p->arena, tok->line, tok->column, token_to_binop(tok->kind), left, right);
    }
    return left;
}

static AstNode *parse_logical_and(Parser *p) {
    AstNode *left = parse_comparison(p);
    while (parser_check(p, TOK_ANDAND)) {
        Token *tok = parser_advance(p);
        AstNode *right = parse_comparison(p);
        left = ast_binary(p->arena, tok->line, tok->column, OP_AND, left, right);
    }
    return left;
}

static AstNode *parse_logical_or(Parser *p) {
    AstNode *left = parse_logical_and(p);
    while (parser_check(p, TOK_OROR)) {
        Token *tok = parser_advance(p);
        AstNode *right = parse_logical_and(p);
        left = ast_binary(p->arena, tok->line, tok->column, OP_OR, left, right);
    }
    return left;
}

static AstNode *parse_term(Parser *p) {
    AstNode *left = parse_logical_or(p);
    while (parser_check(p, TOK_PLUS) || parser_check(p, TOK_MINUS)) {
        Token *tok = parser_advance(p);
        AstNode *right = parse_comparison(p);
        left = ast_binary(p->arena, tok->line, tok->column, token_to_binop(tok->kind), left, right);
    }
    return left;
}

static AstNode *parse_factor(Parser *p) {
    AstNode *left = parse_term(p);
    while (parser_check(p, TOK_STAR) || parser_check(p, TOK_SLASH)) {
        Token *tok = parser_advance(p);
        AstNode *right = parse_term(p);
        left = ast_binary(p->arena, tok->line, tok->column, token_to_binop(tok->kind), left, right);
    }
    return left;
}

static AstNode *parse_expression(Parser *p) {
    return parse_factor(p);
}

static AstNode *parse_var_decl(Parser *p) {
    Token *name_tok = parser_consume(p, TOK_IDENTIFIER, "expected variable name");
    if (!name_tok) {
        return NULL;
    }
    parser_consume(p, TOK_COLON, "expected ':' after variable name");
    ViperType type = parse_type(p);
    parser_consume(p, TOK_EQUAL, "expected '=' after type");
    AstNode *value = parse_expression(p);
    char *name = token_text(p->arena, name_tok);
    return ast_var_decl(p->arena, name_tok->line, name_tok->column, name, type, value);
}

static AstNode *parse_assign(Parser *p) {
    Token *name_tok = parser_consume(p, TOK_IDENTIFIER, "expected variable name");
    if (!name_tok) {
        return NULL;
    }
    parser_consume(p, TOK_EQUAL, "expected '=' in assignment");
    AstNode *value = parse_expression(p);
    char *name = token_text(p->arena, name_tok);
    return ast_assign(p->arena, name_tok->line, name_tok->column, name, value);
}

static AstNode *parse_if(Parser *p) {
    Token *tok = parser_consume(p, TOK_IF, "expected 'if'");
    if (!tok) {
        return NULL;
    }
    AstNode *cond = parse_expression(p);
    AstNode *then_b = parse_block(p);
    AstNode *else_b = NULL;
    if (parser_match(p, TOK_ELSE)) {
        else_b = parse_block(p);
    }
    return ast_if(p->arena, tok->line, tok->column, cond, then_b, else_b);
}

static AstNode *parse_for(Parser *p);
static AstNode *parse_while(Parser *p);
static AstNode *parse_open(Parser *p);
static AstNode *parse_module_decl(Parser *p);
static AstNode *parse_struct_decl(Parser *p);

static AstNode *parse_struct_decl(Parser *p) {
    Token *tok = parser_consume(p, TOK_STRUCT, "expected 'struct'");
    if (!tok) {
        return NULL;
    }
    Token *name_tok = parser_consume(p, TOK_IDENTIFIER, "expected struct name");
    if (!name_tok) {
        return NULL;
    }
    parser_consume(p, TOK_LBRACE, "expected '{' after struct name");
    skip_newlines(p);
    AstParam *fields = NULL;
    int field_count = 0;
    int field_cap = 0;
    while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
        if (parser_match(p, TOK_NEWLINE)) {
            continue;
        }
        Token *field_tok = parser_consume(p, TOK_IDENTIFIER, "expected field name");
        if (!field_tok) {
            break;
        }
        parser_consume(p, TOK_COLON, "expected ':' after field name");
        ViperType ftype = parse_type(p);
        if (field_count >= field_cap) {
            field_cap = field_cap == 0 ? 4 : field_cap * 2;
            fields = (AstParam *)realloc(fields, (size_t)field_cap * sizeof(AstParam));
        }
        fields[field_count].name = token_text(p->arena, field_tok);
        fields[field_count].type = ftype;
        field_count++;
        skip_newlines(p);
    }
    parser_consume(p, TOK_RBRACE, "expected '}' after struct fields");
    char *name = token_text(p->arena, name_tok);
    return ast_struct_decl(p->arena, tok->line, tok->column, name, fields, field_count);
}

static AstNode *parse_while(Parser *p) {
    Token *tok = parser_consume(p, TOK_WHILE, "expected 'while'");
    if (!tok) {
        return NULL;
    }
    AstNode *cond = parse_expression(p);
    AstNode *body = parse_block(p);
    return ast_while(p->arena, tok->line, tok->column, cond, body);
}

static AstNode *parse_open(Parser *p) {
    Token *tok = parser_consume(p, TOK_OPEN, "expected 'open'");
    if (!tok) {
        return NULL;
    }
    Token *name_tok = parser_consume_name(p, "expected module name after 'open'");
    if (!name_tok) {
        return NULL;
    }
    char *name = token_text(p->arena, name_tok);
    return ast_open(p->arena, tok->line, tok->column, name);
}

static AstNode *parse_module_decl(Parser *p) {
    Token *tok = parser_consume(p, TOK_MODULE, "expected 'module'");
    if (!tok) {
        return NULL;
    }
    Token *name_tok = parser_consume_name(p, "expected module name");
    if (!name_tok) {
        return NULL;
    }
    char *name = token_text(p->arena, name_tok);
    return ast_module_decl(p->arena, tok->line, tok->column, name);
}

static AstNode *parse_for(Parser *p) {
    Token *tok = parser_consume(p, TOK_FOR, "expected 'for'");
    if (!tok) {
        return NULL;
    }
    Token *name_tok = parser_consume(p, TOK_IDENTIFIER, "expected loop variable");
    parser_consume(p, TOK_IN, "expected 'in' after loop variable");
    AstNode *start = parse_expression(p);
    parser_consume(p, TOK_DOTDOT, "expected '..' in for loop");
    AstNode *end = parse_expression(p);
    AstNode *body = parse_block(p);
    char *name = token_text(p->arena, name_tok);
    return ast_for(p->arena, tok->line, tok->column, name, start, end, body);
}

static AstParam *parse_params(Parser *p, int *out_count) {
    *out_count = 0;
    if (parser_check(p, TOK_RPAREN)) {
        return NULL;
    }
    AstParam *params = NULL;
    int count = 0;
    int cap = 0;
    do {
        Token *name_tok = parser_consume(p, TOK_IDENTIFIER, "expected parameter name");
        if (!name_tok) {
            break;
        }
        parser_consume(p, TOK_COLON, "expected ':' after parameter name");
        ViperType type = parse_type(p);
        if (count >= cap) {
            cap = cap == 0 ? 4 : cap * 2;
            params = (AstParam *)realloc(params, (size_t)cap * sizeof(AstParam));
        }
        params[count].name = token_text(p->arena, name_tok);
        params[count].type = type;
        count++;
    } while (parser_match(p, TOK_COMMA));
    *out_count = count;
    return params;
}

static AstNode *parse_fn_decl(Parser *p) {
    bool exported = false;
    if (parser_match(p, TOK_EXPORT)) {
        exported = true;
    }
    Token *tok = parser_consume(p, TOK_FN, "expected 'fn'");
    if (!tok) {
        return NULL;
    }
    Token *name_tok = parser_consume(p, TOK_IDENTIFIER, "expected function name");
    parser_consume(p, TOK_LPAREN, "expected '(' after function name");
    int param_count = 0;
    AstParam *params = parse_params(p, &param_count);
    parser_consume(p, TOK_RPAREN, "expected ')' after parameters");
    parser_consume(p, TOK_ARROW, "expected '->' after parameters");
    ViperType return_type = parse_type(p);
    AstNode *body = parse_block(p);
    char *name = token_text(p->arena, name_tok);
    return ast_fn_decl(p->arena, tok->line, tok->column, name, params, param_count, return_type, body, exported,
                       p->current_module);
}

static AstNode *parse_block(Parser *p) {
    Token *tok = parser_consume(p, TOK_LBRACE, "expected '{'");
    if (!tok) {
        return NULL;
    }
    skip_newlines(p);
    AstNode **stmts = NULL;
    int count = 0;
    int cap = 0;
    while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
        if (parser_match(p, TOK_NEWLINE)) {
            continue;
        }
        if (count >= cap) {
            cap = cap == 0 ? 8 : cap * 2;
            stmts = (AstNode **)realloc(stmts, (size_t)cap * sizeof(AstNode *));
        }
        stmts[count++] = parse_statement(p);
        skip_newlines(p);
    }
    parser_consume(p, TOK_RBRACE, "expected '}'");
    AstNode *block = ast_block(p->arena, tok->line, tok->column, stmts, count);
    free(stmts);
    return block;
}

static AstNode *parse_print(Parser *p) {
    Token *tok = parser_consume(p, TOK_IDENTIFIER, "expected 'print'");
    if (!tok) {
        return NULL;
    }
    char *name = token_text(p->arena, tok);
    if (strcmp(name, "print") != 0) {
        parser_error(p, tok, "expected print statement");
        return NULL;
    }
    parser_consume(p, TOK_LPAREN, "expected '(' after print");
    AstNode *expr = parse_expression(p);
    parser_consume(p, TOK_RPAREN, "expected ')' after print expression");
    return ast_print(p->arena, tok->line, tok->column, expr);
}

static AstNode *parse_return(Parser *p) {
    Token *tok = parser_consume(p, TOK_RETURN, "expected 'return'");
    if (!tok) {
        return NULL;
    }
    AstNode *value = parse_expression(p);
    return ast_return(p->arena, tok->line, tok->column, value);
}

static void parser_sync(Parser *p) {
    while (!parser_check(p, TOK_EOF)) {
        if (parser_check(p, TOK_NEWLINE) || parser_check(p, TOK_RBRACE)) {
            break;
        }
        parser_advance(p);
    }
}

static char *token_text_strdup(Token *tok) {
    char *buf = (char *)malloc((size_t)tok->length + 1);
    if (!buf) {
        return NULL;
    }
    memcpy(buf, tok->start, (size_t)tok->length);
    buf[tok->length] = '\0';
    return buf;
}

static AstNode *parse_statement(Parser *p) {
    Token *tok = parser_current(p);
    if (tok->kind == TOK_EXPORT || tok->kind == TOK_FN) {
        return parse_fn_decl(p);
    }
    if (tok->kind == TOK_IF) {
        return parse_if(p);
    }
    if (tok->kind == TOK_FOR) {
        return parse_for(p);
    }
    if (tok->kind == TOK_WHILE) {
        return parse_while(p);
    }
    if (tok->kind == TOK_OPEN) {
        return parse_open(p);
    }
    if (tok->kind == TOK_MODULE) {
        return parse_module_decl(p);
    }
    if (tok->kind == TOK_RETURN) {
        return parse_return(p);
    }
    if (tok->kind == TOK_BREAK) {
        parser_advance(p);
        return ast_break(p->arena, tok->line, tok->column);
    }
    if (tok->kind == TOK_CONTINUE) {
        parser_advance(p);
        return ast_continue(p->arena, tok->line, tok->column);
    }
    if (tok->kind == TOK_STRUCT) {
        return parse_struct_decl(p);
    }
    if (tok->kind == TOK_LBRACE) {
        return parse_block(p);
    }
    if (tok->kind == TOK_IDENTIFIER) {
        if (parser_peek(p, 1)->kind == TOK_LPAREN) {
            char buf[64];
            int len = tok->length < 63 ? tok->length : 63;
            memcpy(buf, tok->start, (size_t)len);
            buf[len] = '\0';
            if (strcmp(buf, "print") == 0) {
                return parse_print(p);
            }
        }
        if (parser_peek(p, 1)->kind == TOK_DOT && parser_peek(p, 2)->kind == TOK_IDENTIFIER &&
            parser_peek(p, 3)->kind == TOK_LPAREN) {
            AstNode *expr = parse_expression(p);
            return ast_expr_stmt(p->arena, tok->line, tok->column, expr);
        }
        if (parser_peek(p, 1)->kind == TOK_COLON) {
            return parse_var_decl(p);
        }
        if (parser_peek(p, 1)->kind == TOK_EQUAL) {
            return parse_assign(p);
        }
    }
    AstNode *expr = parse_expression(p);
    if (!expr && p->diag->error_count > 0) {
        parser_sync(p);
        return NULL;
    }
    return ast_expr_stmt(p->arena, tok->line, tok->column, expr);
}

void parser_init(Parser *p, TokenList *tokens, Arena *arena, DiagContext *diag, const char *file,
                 const char *default_module) {
    p->tokens = tokens;
    p->pos = 0;
    p->arena = arena;
    p->diag = diag;
    p->file = file;
    p->default_module = default_module;
    p->current_module = default_module;
}

bool parse_file_header(Parser *p, FileHeader *out) {
    memset(out, 0, sizeof(*out));
    skip_newlines(p);
    if (parser_check(p, TOK_MODULE)) {
        parser_advance(p);
        Token *name_tok = parser_consume_name(p, "expected module name");
        if (!name_tok) {
            return false;
        }
        out->module_name = token_text_strdup(name_tok);
        skip_newlines(p);
    }
    while (parser_check(p, TOK_OPEN)) {
        parser_advance(p);
        Token *mod_tok = parser_consume_name(p, "expected module name after 'open'");
        if (!mod_tok) {
            return false;
        }
        char *open_name = token_text_strdup(mod_tok);
        if (!open_name) {
            return false;
        }
        char **opens = (char **)realloc(out->opens, (size_t)(out->open_count + 1) * sizeof(char *));
        if (!opens) {
            free(open_name);
            return false;
        }
        out->opens = opens;
        out->opens[out->open_count++] = open_name;
        skip_newlines(p);
    }
    return true;
}

AstNode *parse_program(Parser *p) {
    skip_newlines(p);
    const char *module_name = p->default_module;
    AstNode **stmts = NULL;
    int count = 0;
    int cap = 0;

    if (parser_check(p, TOK_MODULE)) {
        AstNode *mod = parse_module_decl(p);
        if (mod) {
            module_name = mod->as.module_decl.name;
            p->current_module = module_name;
            if (count >= cap) {
                cap = cap == 0 ? 16 : cap * 2;
                stmts = (AstNode **)realloc(stmts, (size_t)cap * sizeof(AstNode *));
            }
            stmts[count++] = mod;
        }
        skip_newlines(p);
    }

    while (parser_check(p, TOK_OPEN)) {
        AstNode *open = parse_open(p);
        if (open) {
            if (count >= cap) {
                cap = cap == 0 ? 16 : cap * 2;
                stmts = (AstNode **)realloc(stmts, (size_t)cap * sizeof(AstNode *));
            }
            stmts[count++] = open;
        }
        skip_newlines(p);
    }

    p->current_module = module_name;

    while (!parser_check(p, TOK_EOF)) {
        if (parser_match(p, TOK_NEWLINE)) {
            continue;
        }
        if (count >= cap) {
            cap = cap == 0 ? 16 : cap * 2;
            stmts = (AstNode **)realloc(stmts, (size_t)cap * sizeof(AstNode *));
        }
        stmts[count++] = parse_statement(p);
        skip_newlines(p);
    }
    AstNode *program = ast_program(p->arena, module_name, stmts, count);
    free(stmts);
    return program;
}

bool parse_fn_signatures_only(Parser *p, AstNode ***out_nodes, int *out_count) {
    *out_nodes = NULL;
    *out_count = 0;
    skip_newlines(p);
    const char *module_name = p->default_module;
    if (parser_check(p, TOK_MODULE)) {
        parser_advance(p);
        Token *name_tok = parser_consume_name(p, "expected module name");
        if (name_tok) {
            module_name = token_text(p->arena, name_tok);
        }
        skip_newlines(p);
    }
    while (parser_check(p, TOK_OPEN)) {
        parser_advance(p);
        parser_consume_name(p, "expected module name after 'open'");
        skip_newlines(p);
    }
    p->current_module = module_name;

    AstNode **nodes = NULL;
    int count = 0;
    int cap = 0;
    while (!parser_check(p, TOK_EOF)) {
        if (parser_match(p, TOK_NEWLINE)) {
            continue;
        }
        if (parser_check(p, TOK_EXPORT) || parser_check(p, TOK_FN)) {
            AstNode *fn = parse_fn_decl(p);
            if (fn) {
                if (count >= cap) {
                    cap = cap == 0 ? 8 : cap * 2;
                    nodes = (AstNode **)realloc(nodes, (size_t)cap * sizeof(AstNode *));
                }
                nodes[count++] = fn;
            }
        } else {
            parse_statement(p);
        }
        skip_newlines(p);
    }
    *out_nodes = nodes;
    *out_count = count;
    return p->diag->error_count == 0;
}
