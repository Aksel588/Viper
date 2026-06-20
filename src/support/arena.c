#include "arena.h"

#include <stdlib.h>
#include <string.h>

bool arena_init(Arena *arena, size_t capacity) {
    arena->buffer = (char *)malloc(capacity);
    if (!arena->buffer) {
        return false;
    }
    arena->capacity = capacity;
    arena->offset = 0;
    return true;
}

void arena_reset(Arena *arena) {
    arena->offset = 0;
}

void arena_free(Arena *arena) {
    free(arena->buffer);
    arena->buffer = NULL;
    arena->capacity = 0;
    arena->offset = 0;
}

void *arena_alloc(Arena *arena, size_t size) {
    size_t aligned = (size + 7) & ~(size_t)7;
    if (arena->offset + aligned > arena->capacity) {
        return NULL;
    }
    void *ptr = arena->buffer + arena->offset;
    arena->offset += aligned;
    return ptr;
}

char *arena_strdup(Arena *arena, const char *str) {
    size_t len = strlen(str) + 1;
    char *copy = (char *)arena_alloc(arena, len);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, str, len);
    return copy;
}
