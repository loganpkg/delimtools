/*
 * Copyright (c) 2021 Logan Ryan McLintock
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* File system operations */

#ifdef __linux__
/* For strdup */
#define _XOPEN_SOURCE 500
/* For readdir macros: DT_DIR and DT_REG */
#define _DEFAULT_SOURCE
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#include <Windows.h>
#else
#include <unistd.h>
#include <libgen.h>
#include <dirent.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../gen/gen.h"
#include "../random/random.h"
#include "fs.h"


int is_dir(char *dn)
{
    /*
     * Returns 1 if dn is a directory.
     * Returns 0 if there was an error, or dn is not a directory.
     */
    struct stat st;
    if (stat(dn, &st))
        return 0;               /* Error */

#ifndef S_ISDIR
#define S_ISDIR(m) ((m & S_IFMT) == S_IFDIR)
#endif

    if (S_ISDIR(st.st_mode))
        return 1;               /* Yes. Is a directory. */
    else
        return 0;               /* No. Not a directory. */
}

int filesize(char *fn, size_t * fs)
{
    /* Gets the filesize of a filename */
    struct stat st;
    if (stat(fn, &st))
        return 1;

#ifndef S_ISREG
#define S_ISREG(m) ((m & S_IFMT) == S_IFREG)
#endif

    if (!S_ISREG(st.st_mode))
        return 1;
    if (st.st_size < 0)
        return 1;
    *fs = st.st_size;
    return 0;
}

char *file_to_str(char *fn)
{
    /* Reads a file fn to a dynamically allocated string */
    FILE *fp;
    size_t fs;
    char *str;
    if (filesize(fn, &fs))
        return NULL;
    if (aof(fs, 1))
        return NULL;
    if ((fp = fopen(fn, "rb")) == NULL)
        return NULL;
    if ((str = malloc(fs + 1)) == NULL) {
        fclose(fp);
        return NULL;
    }
    if (fread(str, 1, fs, fp) != fs) {
        fclose(fp);
        free(str);
        return NULL;
    }
    if (fclose(fp)) {
        free(str);
        return NULL;
    }
    *(str + fs) = '\0';
    return str;
}

int walk_dir(char *dir_name, void *info,
             int (*process_file) (char *, void *))
{
    int ret = 0;
#ifdef _WIN32
    WIN32_FIND_DATA wfd;
    HANDLE hdl;
#else
    DIR *dirp = NULL;
    struct dirent *entry;
    unsigned char dt;
#endif
    char *fn = NULL, *path_name = NULL;

#ifdef _WIN32
    if ((hdl = FindFirstFile(dir_name, &wfd)) == INVALID_HANDLE_VALUE)
        quit();
    do {
        if ((fn = strdup(wfd.cFileName)) == NULL)
            quit();

#else
    if ((dirp = opendir(dir_name)) == NULL)
        quit();
    while ((entry = readdir(dirp)) != NULL) {
        if ((fn = strdup(entry->d_name)) == NULL)
            quit();
        dt = entry->d_type;
#endif
        /* Concatenate file path */
        if ((path_name = concat(dir_name, DIRSEP_STR, fn, NULL)) == NULL)
            quit();

#ifdef _WIN32
        if (wfd.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY) {
#else
        if (dt == DT_DIR) {
#endif
            if (strcmp(fn, ".") && strcmp(fn, "..")
                && walk_dir(path_name, info, process_file))
                quit();
#ifdef _WIN32
        } else if (wfd.dwFileAttributes == FILE_ATTRIBUTE_NORMAL) {
#else
        } else if (dt == DT_REG) {
#endif
            if ((*process_file) (path_name, info))
                quit();
        } else {
            /* Not a directory or regular (normal) file */
            quit();
        }
        free(fn);
        fn = NULL;
        free(path_name);
        path_name = NULL;
#ifdef _WIN32
    }
    while (FindNextFile(hdl, &wfd));
#else
    }
#endif

  clean_up:
    free(fn);
    free(path_name);

#ifdef _WIN32
    if (!FindClose(hdl))
        ret = 1;
#else
    if (closedir(dirp))
        ret = 1;
#endif

    return ret;
}

int atomic_write(char *fn, void *info,
                 int (*write_details) (FILE *, void *))
{
    /* Performs atomic writes on POSIX systems */
    int ret = 0;
    char *fn_tilde = NULL;
    FILE *fp = NULL;
    size_t fn_len;

#ifndef _WIN32
    char *fn_copy = NULL;
    char *dir;
    int fd;
    int d_fd = -1;
    struct stat st;
#endif

    /* No filename */
    if (fn == NULL || !(fn_len = strlen(fn)))
        quit();
    if (aof(fn_len, 2))
        quit();
    if ((fn_tilde = malloc(fn_len + 2)) == NULL)
        quit();
    memcpy(fn_tilde, fn, fn_len);
    *(fn_tilde + fn_len) = '~';
    *(fn_tilde + fn_len + 1) = '\0';
    if ((fp = fopen(fn_tilde, "wb")) == NULL)
        quit();

    (*write_details) (fp, info);

    if (fflush(fp))
        quit();

#ifndef _WIN32
    /* If original file exists, then apply its permissions to the new file */
    if (!stat(fn, &st) && S_ISREG(st.st_mode)
        && chmod(fn_tilde, st.st_mode & 0777))
        quit();
    if ((fd = fileno(fp)) == -1)
        quit();
    if (fsync(fd))
        quit();
#endif

    if (fclose(fp))
        quit();
    fp = NULL;

#ifndef _WIN32
    if ((fn_copy = strdup(fn)) == NULL)
        quit();
    if ((dir = dirname(fn_copy)) == NULL)
        quit();
    if ((d_fd = open(dir, O_RDONLY)) == -1)
        quit();
    if (fsync(d_fd))
        quit();
#endif

#ifdef _WIN32
    /* rename does not overwrite an existing file */
    errno = 0;
    if (remove(fn) && errno != ENOENT)
        quit();
#endif

    /* Atomic on POSIX systems */
    if (rename(fn_tilde, fn))
        quit();

#ifndef _WIN32
    if (fsync(d_fd))
        quit();
#endif

  clean_up:
    if (fn_tilde != NULL)
        free(fn_tilde);
    if (fp != NULL && fclose(fp))
        ret = 1;
#ifndef _WIN32
    if (fn_copy != NULL)
        free(fn_copy);
    if (d_fd != -1 && close(d_fd))
        ret = 1;
#endif

    /* Fail */
    if (ret)
        return 1;

    return 0;
}

int cp_file(char *from_file, char *to_file)
{
    int ret = 0;
    FILE *fp_from = NULL;
    FILE *fp_to = NULL;
    size_t fs, fs_left, io_size;
    char *p = NULL;

    if (filesize(from_file, &fs))
        return 1;

    if ((p = malloc(fs > BUFSIZ ? BUFSIZ : fs)) == NULL)
        return 1;

    if ((fp_from = fopen(from_file, "rb")) == NULL) {
        ret = 1;
        goto clean_up;
    }

    if ((fp_to = fopen(to_file, "wb")) == NULL) {
        ret = 1;
        goto clean_up;
    }

    fs_left = fs;

    while (fs_left) {
        io_size = min(BUFSIZ, fs_left);
        if (fread(p, 1, io_size, fp_from) != io_size) {
            ret = 1;
            goto clean_up;
        }
        if (fwrite(p, 1, io_size, fp_to) != io_size) {
            ret = 1;
            goto clean_up;
        }
        fs_left -= io_size;
    }

  clean_up:
    free(p);
    if (fp_from != NULL && fclose(fp_from))
        ret = 1;
    if (fp_to != NULL && fclose(fp_to))
        ret = 1;

    return ret;
}

int exists(char *fn)
{
    return !access(fn, F_OK);
}

int read_pair_file(char *fn, void *info,
                   int (*process_pair) (char *, char *, void *))
{
    /*
     * Read file with '\0' delimiter and entries in pairs.
     * Perform an operation on each pair {name, def}.
     */
    int ret = 0;
    FILE *fp;
    char *p = NULL, *q, *q_stop, *name, *def;
    size_t fs;
    if (filesize(fn, &fs))
        return 1;
    if (!fs)
        return 0;               /* Nothing to do */
    if ((fp = fopen(fn, "rb")) == NULL)
        quit();
    if ((p = malloc(fs)) == NULL)
        quit();
    if (fread(p, 1, fs, fp) != fs)
        quit();

    /* Test that data ends in '\0' */
    if (*(p + fs - 1) != '\0')
        quit();

    q = p;
    q_stop = p + fs - 1;

    while (1) {
        name = q;
        if ((q = memchr(q, '\0', q_stop - q + 1)) == q_stop)
            quit();
        ++q;
        def = q;
        if ((*process_pair) (name, def, info))
            quit();
        if ((q = memchr(q, '\0', q_stop - q + 1)) == q_stop)
            break;
        ++q;
    }

  clean_up:
    if (fp != NULL && fclose(fp))
        ret = 1;
    if (p != NULL)
        free(p);
    return ret;
}

int make_subdirs(char *file_path)
{
    /* Makes the subdirectories leading up to the file in file_path */
    char *p, *q;
    if ((p = strdup(file_path)) == NULL)
        return 1;
    q = p;
    while ((q = strchr(q, DIRSEP_CH)) != NULL) {
        *q = '\0';
        if (!is_dir(p) && mkdir(p)) {
            free(p);
            return 1;
        }
        *q = DIRSEP_CH;
        ++q;
    }
    free(p);
    return 0;
}

int create_new_file(char *fn)
{
    /*
     * Creates a new file fn.
     * Returns 0 on success.
     * Returns 2 if the file fn already exists (failure).
     * Otherwise returns 1 for all other failures.
     */
    int fd;
    errno = 0;
#ifdef _WIN32
    if (_sopen_s
        (&fd, fn, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _SH_DENYRW,
         _S_IREAD | _S_IWRITE))
#else
    if ((fd =
         open(fn, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR)) == -1)
#endif
    {
        if (errno == EEXIST)
            return 2;
        else
            return 1;
    }
#ifdef _WIN32
    if (_close(fd))
#else
    if (close(fd))
#endif
        return 1;
    return 0;
}

int create_new_dir(char *dn)
{
    /*
     * Creates a new directory dn.
     * Returns 0 on success.
     * Returns 2 if the directory dn already exists (failure).
     * Otherwise returns 1 for all other failures.
     */
    errno = 0;
    if (mkdir(dn))
    {
        if (errno == EEXIST)
            return 2;
        else
            return 1;
    }
    return 0;
}

char *make_tmp(char *in_dir, int dir)
{
    /*
     * Creates a temporary directory (if dir is non-zero)
     * or file under in_dir.
     * Need to free after use. Returns NULL on failure.
     */
    char *name;
    char *path;
    char *d;
    size_t try = 10;            /* Number of times to try */
    int r;

    d = in_dir == NULL || *in_dir == '\0' ? "." : in_dir;

    while (try--) {
        if ((name = random_alnum_str(16)) == NULL)
            return NULL;
        if ((path = concat(d, DIRSEP_STR, name, NULL)) == NULL) {
            free(name);
            return NULL;
        }
        free(name);

        if (dir) {
            /* Create a temporary directory */
            if ((r = create_new_dir(path)) != 0) {
                free(path);
                if (r != 2)
                    return NULL;
            } else {
                /* Success */
                return path;
            }
        } else {
            /* Create a temporary file */
            if ((r = create_new_file(path)) != 0) {
                free(path);
                if (r != 2)
                    return NULL;
            } else {
                /* Success */
                return path;
            }
        }
    }
    /* All tries failed */
    return NULL;
}
