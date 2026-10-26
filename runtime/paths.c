#define _POSIX_C_SOURCE 200809L
#include "paths.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int local_directory(const char *name) {
    struct stat st;
    if (mkdir(name, 0755) && errno != EEXIST) return -1;
    if (lstat(name, &st)) return -1;
    if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return -1; }
    return 0;
}

int ds_paths_init(void) {
    char root[PATH_MAX], path[PATH_MAX];
    ssize_t length = readlink("/proc/self/exe", root, sizeof root - 1);
    if (length < 0) return -1;
    if ((size_t)length == sizeof root - 1) { errno = ENAMETOOLONG; return -1; }
    root[length] = '\0';
    char *slash = strrchr(root, '/');
    if (!slash) { errno = EINVAL; return -1; }
    if (slash == root) slash[1] = '\0'; else *slash = '\0';
    if (chdir(root)) return -1;
    const char *dirs[] = {"saves", "config", "userdata", "cache", "tmp"};
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; ++i)
        if (local_directory(dirs[i])) return -1;
    const struct { const char *key, *relative; } env[] = {
        {"DS_ROOT", ""}, {"HOME", ""}, {"DS_SAVE_DIR", "saves"},
        {"XDG_CONFIG_HOME", "config"}, {"XDG_DATA_HOME", "userdata"},
        {"XDG_STATE_HOME", "userdata"}, {"XDG_CACHE_HOME", "cache"},
        {"TMPDIR", "tmp"}, {"DS_PAD_CONFIG", "controller-buttons.cfg"}
    };
    for (size_t i = 0; i < sizeof env / sizeof env[0]; ++i) {
        int n = snprintf(path, sizeof path, "%s%s%s", root,
                         *env[i].relative ? "/" : "", env[i].relative);
        if (n < 0 || (size_t)n >= sizeof path) { errno = ENAMETOOLONG; return -1; }
        if (setenv(env[i].key, path, 1)) return -1;
    }
    /* XDG_RUNTIME_DIR is a firmware service socket location, not game storage. */
    return 0;
}
