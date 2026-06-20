#include "paths.h"

#include "viper.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

const char *viper_stdlib_path(void) {
    const char *env = getenv("VIPER_PATH");
    if (env && env[0] != '\0') {
        return env;
    }
    return VIPER_DEFAULT_LIB;
}

bool viper_path_exists(const char *path) {
    if (!path || path[0] == '\0') {
        return false;
    }
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}
