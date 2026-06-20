#ifndef VIPER_DISCOVERY_H
#define VIPER_DISCOVERY_H

#include <stdbool.h>

#include "diagnostics.h"

typedef struct FileList {
    char **paths;
    int count;
} FileList;

bool path_is_vp_file(const char *path);
bool discover_vp_files_in_dir_of(const char *file_path, FileList *out, DiagContext *diag);
bool discover_vp_files(const char *directory, FileList *out, DiagContext *diag);
bool discover_vp_files_recursive(const char *directory, FileList *out, DiagContext *diag);
void file_list_free(FileList *list);
char *read_file_contents(const char *path, long *out_length, DiagContext *diag);

#endif
