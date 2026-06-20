#include "diagnostics.h"
#include "source.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static bool diag_grow(DiagContext *ctx) {
    int new_cap = ctx->capacity == 0 ? 8 : ctx->capacity * 2;
    Diagnostic *items = (Diagnostic *)realloc(ctx->items, (size_t)new_cap * sizeof(Diagnostic));
    if (!items) {
        return false;
    }
    ctx->items = items;
    ctx->capacity = new_cap;
    return true;
}

bool diag_init(DiagContext *ctx, int initial_capacity) {
    ctx->items = (Diagnostic *)calloc((size_t)initial_capacity, sizeof(Diagnostic));
    if (!ctx->items) {
        return false;
    }
    ctx->count = 0;
    ctx->capacity = initial_capacity;
    ctx->error_count = 0;
    return true;
}

void diag_free(DiagContext *ctx) {
    for (int i = 0; i < ctx->count; i++) {
        free(ctx->items[i].message);
        free(ctx->items[i].file);
    }
    free(ctx->items);
    ctx->items = NULL;
    ctx->count = 0;
    ctx->capacity = 0;
    ctx->error_count = 0;
}

void diag_emit(DiagContext *ctx, DiagLevel level, const char *file, int line, int column,
               const char *fmt, ...) {
    if (ctx->count >= ctx->capacity && !diag_grow(ctx)) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (needed < 0) {
        va_end(args);
        return;
    }

    char *message = (char *)malloc((size_t)needed + 1);
    if (!message) {
        va_end(args);
        return;
    }
    vsnprintf(message, (size_t)needed + 1, fmt, args);
    va_end(args);

    char *file_copy = file ? strdup(file) : NULL;

    Diagnostic *d = &ctx->items[ctx->count++];
    d->level = level;
    d->line = line;
    d->column = column;
    d->message = message;
    d->file = file_copy;

    if (level == DIAG_ERROR) {
        ctx->error_count++;
    }
}

void diag_print_all(const DiagContext *ctx, FILE *out) {
    for (int i = 0; i < ctx->count; i++) {
        const Diagnostic *d = &ctx->items[i];
        const char *level_str = d->level == DIAG_ERROR ? "error" : "warning";
        if (d->file) {
            fprintf(out, "%s:%d:%d: %s: %s\n", d->file, d->line, d->column, level_str, d->message);
            const char *line_text = source_mgr_get_line(&g_source_mgr, d->file, d->line);
            if (line_text) {
                fprintf(out, "  %s\n", line_text);
                fprintf(out, "  ");
                int col = d->column > 0 ? d->column : 1;
                for (int c = 1; c < col; c++) {
                    fputc(line_text[c - 1] == '\t' ? '\t' : ' ', out);
                }
                fputc('^', out);
                fputc('\n', out);
            }
        } else {
            fprintf(out, "%d:%d: %s: %s\n", d->line, d->column, level_str, d->message);
        }
    }
}
