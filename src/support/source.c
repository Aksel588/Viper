#include "source.h"

#include <stdlib.h>
#include <string.h>

SourceManager g_source_mgr;

void source_mgr_init(SourceManager *mgr) {
    mgr->paths = NULL;
    mgr->contents = NULL;
    mgr->count = 0;
    mgr->capacity = 0;
}

void source_mgr_free(SourceManager *mgr) {
    for (int i = 0; i < mgr->count; i++) {
        free(mgr->paths[i]);
        free(mgr->contents[i]);
    }
    free(mgr->paths);
    free(mgr->contents);
    mgr->paths = NULL;
    mgr->contents = NULL;
    mgr->count = 0;
    mgr->capacity = 0;
}

void source_mgr_register(SourceManager *mgr, const char *path, char *content) {
    for (int i = 0; i < mgr->count; i++) {
        if (strcmp(mgr->paths[i], path) == 0) {
            free(mgr->contents[i]);
            mgr->contents[i] = content ? strdup(content) : NULL;
            return;
        }
    }
    if (mgr->count >= mgr->capacity) {
        int cap = mgr->capacity == 0 ? 8 : mgr->capacity * 2;
        char **paths = (char **)realloc(mgr->paths, (size_t)cap * sizeof(char *));
        char **contents = (char **)realloc(mgr->contents, (size_t)cap * sizeof(char *));
        if (!paths || !contents) {
            return;
        }
        mgr->paths = paths;
        mgr->contents = contents;
        mgr->capacity = cap;
    }
    mgr->paths[mgr->count] = strdup(path);
    mgr->contents[mgr->count] = content ? strdup(content) : NULL;
    if (!mgr->paths[mgr->count] || (content && !mgr->contents[mgr->count])) {
        free(mgr->paths[mgr->count]);
        free(mgr->contents[mgr->count]);
        return;
    }
    mgr->count++;
}

const char *source_mgr_get_line(const SourceManager *mgr, const char *file, int line) {
    static char line_buf[512];
    for (int i = 0; i < mgr->count; i++) {
        if (strcmp(mgr->paths[i], file) != 0) {
            continue;
        }
        const char *src = mgr->contents[i];
        int cur = 1;
        const char *start = src;
        for (const char *p = src; *p; p++) {
            if (*p == '\n') {
                if (cur == line) {
                    size_t len = (size_t)(p - start);
                    if (len >= sizeof(line_buf)) {
                        len = sizeof(line_buf) - 1;
                    }
                    memcpy(line_buf, start, len);
                    line_buf[len] = '\0';
                    return line_buf;
                }
                cur++;
                start = p + 1;
            }
        }
        if (cur == line) {
            size_t len = strlen(start);
            if (len >= sizeof(line_buf)) {
                len = sizeof(line_buf) - 1;
            }
            memcpy(line_buf, start, len);
            line_buf[len] = '\0';
            return line_buf;
        }
        return NULL;
    }
    return NULL;
}
