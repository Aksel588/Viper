#ifndef VIPER_DIAGNOSTICS_H
#define VIPER_DIAGNOSTICS_H

#include <stdbool.h>
#include <stdio.h>

typedef enum {
    DIAG_ERROR,
    DIAG_WARNING
} DiagLevel;

typedef struct Diagnostic {
    DiagLevel level;
    int line;
    int column;
    char *message;
    char *file;
} Diagnostic;

typedef struct DiagContext {
    Diagnostic *items;
    int count;
    int capacity;
    int error_count;
} DiagContext;

bool diag_init(DiagContext *ctx, int initial_capacity);
void diag_free(DiagContext *ctx);
void diag_emit(DiagContext *ctx, DiagLevel level, const char *file, int line, int column,
               const char *fmt, ...);
void diag_print_all(const DiagContext *ctx, FILE *out);

#endif
