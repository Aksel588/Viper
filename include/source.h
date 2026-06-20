#ifndef VIPER_SOURCE_H
#define VIPER_SOURCE_H

typedef struct SourceManager {
    char **paths;
    char **contents;
    int count;
    int capacity;
} SourceManager;

void source_mgr_init(SourceManager *mgr);
void source_mgr_free(SourceManager *mgr);
void source_mgr_register(SourceManager *mgr, const char *path, char *content);
const char *source_mgr_get_line(const SourceManager *mgr, const char *file, int line);

extern SourceManager g_source_mgr;

#endif
