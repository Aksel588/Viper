#include "module.h"

#include "lexer.h"
#include "parser.h"
#include "source.h"

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *module_name_from_path(const char *path) {
    char *copy = strdup(path);
    if (!copy) {
        return NULL;
    }
    char *base = basename(copy);
    size_t len = strlen(base);
    if (len > 3 && strcmp(base + len - 3, ".vp") == 0) {
        len -= 3;
    }
    char *name = (char *)malloc(len + 1);
    if (name) {
        memcpy(name, base, len);
        name[len] = '\0';
    }
    free(copy);
    return name;
}

void module_index_free(ModuleIndex *idx) {
    if (!idx) {
        return;
    }
    for (int i = 0; i < idx->count; i++) {
        free(idx->entries[i].name);
        free(idx->entries[i].path);
        free(idx->entries[i].declared_name);
    }
    free(idx->entries);
    idx->entries = NULL;
    idx->count = 0;
}

static bool module_index_add(ModuleIndex *idx, const char *name, const char *path, const char *declared) {
    for (int i = 0; i < idx->count; i++) {
        if (strcmp(idx->entries[i].name, name) == 0) {
            return false;
        }
    }
    ModuleEntry *entries = (ModuleEntry *)realloc(idx->entries, (size_t)(idx->count + 1) * sizeof(ModuleEntry));
    if (!entries) {
        return false;
    }
    idx->entries = entries;
    idx->entries[idx->count].name = strdup(name);
    idx->entries[idx->count].path = strdup(path);
    idx->entries[idx->count].declared_name = declared ? strdup(declared) : NULL;
    idx->count++;
    return true;
}

bool module_index_build(const char *root, bool recursive, ModuleIndex *out, DiagContext *diag) {
    memset(out, 0, sizeof(*out));
    FileList files;
    bool ok = recursive ? discover_vp_files_recursive(root, &files, diag)
                        : discover_vp_files(root, &files, diag);
    if (!ok) {
        return false;
    }
    for (int i = 0; i < files.count; i++) {
        FileHeader hdr;
        if (!module_parse_header(files.paths[i], &hdr, diag)) {
            file_list_free(&files);
            return false;
        }
        const char *mod_name = hdr.module_name ? hdr.module_name : module_name_from_path(files.paths[i]);
        if (!mod_name) {
            file_header_free(&hdr);
            file_list_free(&files);
            return false;
        }
        if (!module_index_add(out, mod_name, files.paths[i], hdr.module_name)) {
            diag_emit(diag, DIAG_ERROR, files.paths[i], 0, 0,
                      "duplicate module name '%s' (use explicit 'module' decl to disambiguate)", mod_name);
            if (!hdr.module_name) {
                free((void *)mod_name);
            }
            file_header_free(&hdr);
            file_list_free(&files);
            return false;
        }
        if (!hdr.module_name) {
            free((void *)mod_name);
        }
        file_header_free(&hdr);
    }
    file_list_free(&files);
    return true;
}

bool module_index_merge(ModuleIndex *dest, const ModuleIndex *src, DiagContext *diag) {
    (void)diag;
    for (int i = 0; i < src->count; i++) {
        ModuleEntry *existing = module_index_find(dest, src->entries[i].name);
        if (existing) {
            free(existing->path);
            free(existing->declared_name);
            existing->path = strdup(src->entries[i].path);
            existing->declared_name =
                src->entries[i].declared_name ? strdup(src->entries[i].declared_name) : NULL;
            if (!existing->path) {
                return false;
            }
            continue;
        }
        if (!module_index_add(dest, src->entries[i].name, src->entries[i].path, src->entries[i].declared_name)) {
            return false;
        }
    }
    return true;
}

ModuleEntry *module_index_find(ModuleIndex *idx, const char *name) {
    for (int i = 0; i < idx->count; i++) {
        if (strcmp(idx->entries[i].name, name) == 0) {
            return &idx->entries[i];
        }
    }
    return NULL;
}

ModuleEntry *module_index_find_by_path(ModuleIndex *idx, const char *path) {
    for (int i = 0; i < idx->count; i++) {
        if (strcmp(idx->entries[i].path, path) == 0) {
            return &idx->entries[i];
        }
    }
    return NULL;
}

void file_header_free(FileHeader *hdr) {
    if (!hdr) {
        return;
    }
    free(hdr->module_name);
    for (int i = 0; i < hdr->open_count; i++) {
        free(hdr->opens[i]);
    }
    free(hdr->opens);
    hdr->module_name = NULL;
    hdr->opens = NULL;
    hdr->open_count = 0;
}

bool module_parse_header(const char *path, FileHeader *out, DiagContext *diag) {
    memset(out, 0, sizeof(*out));
    long length = 0;
    char *source = read_file_contents(path, &length, diag);
    if (!source) {
        return false;
    }

    Arena arena;
    if (!arena_init(&arena, 4096)) {
        free(source);
        return false;
    }
    Lexer lex;
    lexer_init(&lex, source, (int)length, path, diag);
    TokenList tokens;
    if (!lexer_tokenize(&lex, &tokens)) {
        arena_free(&arena);
        free(source);
        return false;
    }
    Parser parser;
    char *default_mod = module_name_from_path(path);
    parser_init(&parser, &tokens, &arena, diag, path, default_mod ? default_mod : "main");
    free(default_mod);
    if (!parse_file_header(&parser, out)) {
        token_list_free(&tokens);
        arena_free(&arena);
        free(source);
        return false;
    }
    token_list_free(&tokens);
    arena_free(&arena);
    free(source);
    return diag->error_count == 0;
}

typedef struct DepQueue {
    char **paths;
    int count;
    int cap;
} DepQueue;

static bool dep_queue_contains(DepQueue *q, const char *path) {
    for (int i = 0; i < q->count; i++) {
        if (strcmp(q->paths[i], path) == 0) {
            return true;
        }
    }
    return false;
}

static bool dep_queue_push(DepQueue *q, const char *path) {
    if (dep_queue_contains(q, path)) {
        return true;
    }
    if (q->count >= q->cap) {
        int cap = q->cap == 0 ? 8 : q->cap * 2;
        char **paths = (char **)realloc(q->paths, (size_t)cap * sizeof(char *));
        if (!paths) {
            return false;
        }
        q->paths = paths;
        q->cap = cap;
    }
    q->paths[q->count++] = strdup(path);
    return true;
}

static void dep_queue_free(DepQueue *q) {
    for (int i = 0; i < q->count; i++) {
        free(q->paths[i]);
    }
    free(q->paths);
    q->paths = NULL;
    q->count = 0;
    q->cap = 0;
}

bool module_resolve_deps(const char *entry_path, ModuleIndex *idx, FileList *closure, DiagContext *diag) {
    closure->paths = NULL;
    closure->count = 0;

    DepQueue queue = {0};
    DepQueue visited = {0};

    if (!dep_queue_push(&queue, entry_path)) {
        return false;
    }

    while (queue.count > 0) {
        char *path = queue.paths[--queue.count];
        if (dep_queue_contains(&visited, path)) {
            free(path);
            continue;
        }
        dep_queue_push(&visited, path);

        FileHeader hdr;
        if (!module_parse_header(path, &hdr, diag)) {
            dep_queue_free(&queue);
            dep_queue_free(&visited);
            return false;
        }

        for (int i = 0; i < hdr.open_count; i++) {
            ModuleEntry *mod = module_index_find(idx, hdr.opens[i]);
            if (!mod) {
                diag_emit(diag, DIAG_ERROR, path, 0, 0, "unknown module '%s' in open statement", hdr.opens[i]);
                file_header_free(&hdr);
                dep_queue_free(&queue);
                dep_queue_free(&visited);
                return false;
            }
            if (!dep_queue_push(&queue, mod->path)) {
                file_header_free(&hdr);
                dep_queue_free(&queue);
                dep_queue_free(&visited);
                return false;
            }
        }
        file_header_free(&hdr);
        free(path);
    }

    for (int i = 0; i < visited.count; i++) {
        char **paths = (char **)realloc(closure->paths, (size_t)(closure->count + 1) * sizeof(char *));
        if (!paths) {
            dep_queue_free(&visited);
            file_list_free(closure);
            return false;
        }
        closure->paths = paths;
        closure->paths[closure->count++] = visited.paths[i];
    }
    free(visited.paths);
    return true;
}
