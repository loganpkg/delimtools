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

#ifdef _WIN32
/* For mkdir */
#include <direct.h>
#endif

#ifdef _WIN32
#define DIRSEP_CH '\\'
#define DIRSEP_STR "\\"
#else
#define DIRSEP_CH '/'
#define DIRSEP_STR "/"
#endif

/* Always want directories to have permissions of 0700 */
#ifndef _WIN32
#define mkdir(dn) mkdir(dn, S_IRWXU)
#endif

int is_dir(char *dn);
int filesize(char *fn, size_t * fs);
char *file_to_str(char *fn);
int concat_path(char *res_path, char *dn, char *fn);
int walk_dir(char *dir_name, void *info,
             int (*process_file) (char *, void *));
int atomic_write(char *fn, void *info,
                 int (*write_details) (FILE *, void *));
int cp_file(char *from_file, char *to_file);
int exists(char *fn);
int read_pair_file(char *fn, void *info,
                   int (*process_pair) (char *, char *, void *));
int make_subdirs(char *file_path);
int create_new_file(char *fn);
int create_new_dir(char *dn);
char *make_tmp(char *in_dir, int dir);
