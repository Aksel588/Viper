#include "discovery.h"
#include "source.h"

#include <dirent.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int cmp_paths(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static bool file_list_add(FileList *list, const char *path) {
    char **paths = (char **)realloc(list->paths, (size_t)(list->count + 1) * sizeof(char *));
    if (!paths) {
        return false;
    }
    list->paths = paths;
    list->paths[list->count] = strdup(path);
    if (!list->paths[list->count]) {
        return false;
    }
    list->count++;
    return true;
}

static bool has_vp_extension(const char *name) {
    size_t len = strlen(name);
    return len > 3 && strcmp(name + len - 3, ".vp") == 0;
}

bool path_is_vp_file(const char *path) {
    size_t len = strlen(path);
    return len > 3 && strcmp(path + len - 3, ".vp") == 0;
}

bool discover_vp_files_in_dir_of(const char *file_path, FileList *out, DiagContext *diag) {
    char *copy = strdup(file_path);
    if (!copy) {
        return false;
    }
    char *dir = dirname(copy);
    bool ok = discover_vp_files(dir, out, diag);
    free(copy);
    return ok;
}

bool discover_vp_files(const char *directory, FileList *out, DiagContext *diag) {
    out->paths = NULL;
    out->count = 0;

    DIR *dir = opendir(directory);
    if (!dir) {
        diag_emit(diag, DIAG_ERROR, NULL, 0, 0, "cannot open directory '%s'", directory);
        return false;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (!has_vp_extension(entry->d_name)) {
            continue;
        }

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);

        struct stat st;
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }
        if (!file_list_add(out, path)) {
            closedir(dir);
            return false;
        }
    }
    closedir(dir);

    if (out->count == 0) {
        diag_emit(diag, DIAG_WARNING, NULL, 0, 0, "no .vp files found in '%s'", directory);
        return true;
    }

    qsort(out->paths, (size_t)out->count, sizeof(char *), cmp_paths);
    return true;
}

static bool discover_recursive_inner(const char *directory, FileList *out, DiagContext *diag) {
    DIR *dir = opendir(directory);
    if (!dir) {
        return false;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        struct stat st;
        if (stat(path, &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            if (!discover_recursive_inner(path, out, diag)) {
                closedir(dir);
                return false;
            }
        } else if (S_ISREG(st.st_mode) && has_vp_extension(entry->d_name)) {
            if (!file_list_add(out, path)) {
                closedir(dir);
                return false;
            }
        }
    }
    closedir(dir);
    return true;
}

bool discover_vp_files_recursive(const char *directory, FileList *out, DiagContext *diag) {
    out->paths = NULL;
    out->count = 0;
    if (!discover_recursive_inner(directory, out, diag)) {
        diag_emit(diag, DIAG_ERROR, NULL, 0, 0, "cannot scan directory '%s'", directory);
        return false;
    }
    if (out->count == 0) {
        diag_emit(diag, DIAG_WARNING, NULL, 0, 0, "no .vp files found under '%s'", directory);
    } else {
        qsort(out->paths, (size_t)out->count, sizeof(char *), cmp_paths);
    }
    return true;
}

void file_list_free(FileList *list) {
    if (!list) {
        return;
    }
    for (int i = 0; i < list->count; i++) {
        free(list->paths[i]);
    }
    free(list->paths);
    list->paths = NULL;
    list->count = 0;
}

char *read_file_contents(const char *path, long *out_length, DiagContext *diag) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        diag_emit(diag, DIAG_ERROR, path, 0, 0, "cannot open file");
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        diag_emit(diag, DIAG_ERROR, path, 0, 0, "cannot read file");
        return NULL;
    }
    long len = ftell(f);
    if (len < 0) {
        fclose(f);
        diag_emit(diag, DIAG_ERROR, path, 0, 0, "cannot read file");
        return NULL;
    }
    rewind(f);

    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t read = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[read] = '\0';
    *out_length = (long)read;
    source_mgr_register(&g_source_mgr, path, buf);
    return buf;
}
