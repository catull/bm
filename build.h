#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   include "windows.h"
#else
#   include <sys/stat.h>
#   include <sys/types.h>
#   include <sys/wait.h>
#   include <unistd.h>
#endif // _WIN32

#ifdef _WIN32
typedef struct {
    HANDLE hFind;
    WIN32_FIND_DATA data;
    struct dirent *dirent;
} DIR;

DIR *opendir(const char *dirpath)
{
    assert(dirpath);

    char buffer[MAX_PATH];
    snprintf(buffer, MAX_PATH, "%s\\*", dirpath);

    DIR *dir = (DIR*)calloc(1, sizeof(DIR));

    dir->hFind = FindFirstFile(buffer, &dir->data);
    if (dir->hFind == INVALID_HANDLE_VALUE) {
        goto fail;
    }

    return dir;

fail:
    if (dir) {
        free(dir);
    }

    return NULL;
}

struct dirent *readdir(DIR *dirp)
{
    assert(dirp);

    if (dirp->dirent == NULL) {
        dirp->dirent = (dirent*)calloc(1, sizeof(struct dirent));
    } else {
        if(!FindNextFile(dirp->hFind, &dirp->data)) {
            return NULL;
        }
    }

    memset(dirp->dirent->d_name, 0, sizeof(dirp->dirent->d_name));

    strncpy(
        dirp->dirent->d_name,
        dirp->data.cFileName,
        sizeof(dirp->dirent->d_name) - 1);

    return dirp->dirent;
}

void closedir(DIR *dirp)
{
    assert(dirp);

    FindClose(dirp->hFind);
    if (dirp->dirent) {
        free(dirp->dirent);
    }
    free(dirp);
}
#else
#    include <dirent.h>
#endif // _WIN32

#ifdef _WIN32
#    define PATH_SEP "\\"
#else
#    define PATH_SEP "/"
#endif // _WIN32

#define PATH_SEP_LEN (sizeof(PATH_SEP) - 1)

#define FOREACH_VARGS(param, arg, args, body)               \
    do {                                                    \
        va_start(args, param);                              \
        for (const char *arg = va_arg(args, const char *);  \
             arg != NULL;                                   \
             arg = va_arg(args, const char *))              \
        {                                                   \
            body;                                           \
        }                                                   \
        va_end(args);                                       \
    } while(0)

#define FOREACH_ARRAY(type, item, items, body)  \
    do {                                        \
        for (size_t i = 0;                                              \
             i < sizeof(items) / sizeof((items)[0]);                    \
             ++i)                                                       \
        {                                                               \
            type item = items[i];                                       \
            body;                                                       \
        }                                                               \
    } while(0)

#define FOREACH_FILE_IN_DIR(file, dirpath, body)        \
    do {                                                \
        struct dirent *dp = NULL;                       \
        DIR *dir = opendir(dirpath);                    \
        while (dp = readdir(dir)) {                     \
            const char *file = dp->d_name;              \
            body;                                       \
        }                                               \
        closedir(dir);                                  \
    } while(0)

const char *concat_sep_impl(const char *sep, ...)
{
    const size_t sep_len = strlen(sep);
    size_t length = 0;
    size_t seps_count = 0;

    va_list args;

    FOREACH_VARGS(sep, arg, args, {
        length += strlen(arg);
        seps_count += 1;
    });
    assert(length > 0);

    seps_count -= 1;

    char *result = malloc(length + seps_count * sep_len + 1);

    length = 0;
    FOREACH_VARGS(sep, arg, args, {
        size_t n = strlen(arg);
        memcpy(result + length, arg, n);
        length += n;

        if (seps_count > 0) {
            memcpy(result + length, sep, sep_len);
            length += sep_len;
            seps_count -= 1;
        }
    });

    result[length] = '\0';

    return result;
}

#define CONCAT_SEP(sep, ...) concat_sep_impl(sep, __VA_ARGS__, NULL)

#define PATH(...) CONCAT_SEP(PATH_SEP, __VA_ARGS__)

void mkdirs_impl(int ignore, ...)
{
    size_t length = 0;
    size_t seps_count = 0;

    va_list args;

    FOREACH_VARGS(ignore, arg, args, {
        length += strlen(arg);
        seps_count += 1;
    });

    assert(length > 0);
    seps_count -= 1;

    char *result = malloc(length + seps_count * PATH_SEP_LEN + 1);

    length = 0;
    FOREACH_VARGS(ignore, arg, args, {
        size_t n = strlen(arg);
        memcpy(result + length, arg, n);
        length += n;

        if (seps_count > 0) {
            memcpy(result + length, PATH_SEP, PATH_SEP_LEN);
            length += PATH_SEP_LEN;
            seps_count -= 1;
        }

        result[length] = '\0';

        printf("[INFO] mkdir %s\n", result);
        // TODO: MKDIRS does not work on Windows
        //  https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/mkdir-wmkdir?view=msvc-160
        if (mkdir(result, 0755) < 0) {
            if (errno == EEXIST) {
                fprintf(stderr, "[WARN] directory %s already exists\n",
                        result);
            } else {
                fprintf(stderr, "[ERROR] could not create directory %s: %s\n",
                        result, strerror(errno));
                exit(1);
            }
        }
    });
}

#define MKDIRS(...) mkdirs_impl(69, __VA_ARGS__, NULL)

const char *concat_impl(int ignore, ...)
{
    size_t length = 0;
    va_list args;
    FOREACH_VARGS(ignore, arg, args, {
        length += strlen(arg);
    });

    char *result = malloc(length + 1);

    length = 0;
    FOREACH_VARGS(ignore, arg, args, {
        size_t n = strlen(arg);
        memcpy(result + length, arg, n);
        length += n;
    });
    result[length] = '\0';

    return result;
}

#define CONCAT(...) concat_impl(69, __VA_ARGS__, NULL)

#ifdef _WIN32
void build_h_exec(const char **argv)
{
    // TODO: CMD does not work on Windows
    // https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/execvp-wexecvp?view=msvc-160
    if (_execvp(argv[0], (char * const*) argv) < 0) {
        fprintf(stderr, "[ERROR] could not execute child process: %s\n",
                strerror(errno));
        exit(1);
    }
}
#else
void build_h_exec(const char **argv)
{
    pid_t cpid = fork();
    if (cpid == -1) {
        fprintf(stderr, "[ERROR] could not fork a child process: %s\n",
                strerror(errno));
        exit(1);
    }

    if (cpid == 0) {
        if (execvp(argv[0], (char * const*) argv) < 0) {
            fprintf(stderr, "[ERROR] could not execute child process: %s\n",
                    strerror(errno));
            exit(1);
        }
    } else {
        // TODO: child fail is not properly reported on Linux
        wait(NULL);
    }
}
#endif // _WIN32

void cmd_impl(int ignore, ...)
{
    size_t argc = 0;

    va_list args;
    FOREACH_VARGS(ignore, arg, args, {
        argc += 1;
    });

    const char **argv = malloc(sizeof(const char*) * (argc + 1));

    argc = 0;
    FOREACH_VARGS(ignore, arg, args, {
        argv[argc++] = arg;
    });

    assert(argc >= 1);

    build_h_exec(argv);
}

#define CMD(...)                                                \
    do {                                                        \
        printf("[INFO] %s\n", CONCAT_SEP(" ", __VA_ARGS__));    \
        cmd_impl(69, __VA_ARGS__, NULL);                        \
    } while(0)
