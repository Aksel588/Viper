#ifndef VIPER_ARENA_H
#define VIPER_ARENA_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Arena {
    char *buffer;
    size_t capacity;
    size_t offset;
} Arena;

bool arena_init(Arena *arena, size_t capacity);
void arena_reset(Arena *arena);
void arena_free(Arena *arena);
void *arena_alloc(Arena *arena, size_t size);
char *arena_strdup(Arena *arena, const char *str);

#endif
