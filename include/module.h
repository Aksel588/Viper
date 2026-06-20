#ifndef VIPER_MODULE_H
#define VIPER_MODULE_H

#include <stdbool.h>

#include "diagnostics.h"
#include "discovery.h"

typedef struct ModuleEntry {
    char *name;
    char *path;
    char *declared_name;
} ModuleEntry;

typedef struct ModuleIndex {
    ModuleEntry *entries;
    int count;
} ModuleIndex;

typedef struct FileHeader {
    char *module_name;
    char **opens;
    int open_count;
} FileHeader;

void module_index_free(ModuleIndex *idx);
bool module_index_build(const char *root, bool recursive, ModuleIndex *out, DiagContext *diag);
bool module_index_merge(ModuleIndex *dest, const ModuleIndex *src, DiagContext *diag);
ModuleEntry *module_index_find(ModuleIndex *idx, const char *name);
ModuleEntry *module_index_find_by_path(ModuleIndex *idx, const char *path);
bool module_parse_header(const char *path, FileHeader *out, DiagContext *diag);
void file_header_free(FileHeader *hdr);
bool module_resolve_deps(const char *entry_path, ModuleIndex *idx, FileList *closure, DiagContext *diag);
char *module_name_from_path(const char *path);

#endif
