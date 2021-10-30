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

/* capybara: Backup utility */

#include <sys/stat.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../mods/gen/gen.h"
#include "../../mods/fs/fs.h"
#include "../../mods/buf/buf.h"
#include "../../mods/hashtable/hashtable.h"
#include "../../mods/sha256/sha256.h"

#define INIT_BUF_SIZE BUFSIZ

#define HASH_TABLE_SIZE 262144

struct process_info {
    struct hashtable *ht;
    struct buf *snapshot;
    char *store_dir;
    char *path_to_files;
    char *restore_dir;
};

int process_file_capybara(char *fn, void *info)
{
    int ret = 0;
    char *hash = NULL, *hash_path = NULL;
    struct process_info *p = info;

    if ((hash = sha256(fn)) == NULL)
        mquit("sha256 failed");

    /* Check to see if it is in hash table already */
    if (lookup_entry(p->ht, hash) == NULL) {
        /* Prepare destination path */
        if ((hash_path =
             concat(p->path_to_files, DIRSEP_STR, hash, NULL)) == NULL)
            mquit("concat failed to create hash_path");
        /* Copy file */
        if (cp_file(fn, hash_path))
            mquit("cp_file failed");
        /* Add entry to hash table */
        if (upsert_entry(p->ht, hash, NULL))
            mquit("upsert_entry failed");
        /* Add filename mapping to snapshot list */
        if (put_str(p->snapshot, fn))
            mquit("put_str failed");
        if (put_ch(p->snapshot, '\0'))
            mquit("put_ch failed");
        if (put_str(p->snapshot, hash))
            mquit("put_str failed");
        if (put_ch(p->snapshot, '\0'))
            mquit("put_ch failed");
    }

  clean_up:
    free(hash);
    free(hash_path);
    return ret;
}

int process_pair_capybara(char *name, char *def, void *info)
{
    /* Restores a file, creating the destination directories if necessary */
    int ret = 0;
    struct process_info *p = info;
    char *hash_path = NULL, *restore_path = NULL;

    if ((hash_path =
         concat(p->store_dir, DIRSEP_STR, "files", DIRSEP_STR, def,
                NULL)) == NULL)
        mquit("concat failed to create hash_path");
    if ((restore_path =
         concat(p->restore_dir, DIRSEP_STR, name, NULL)) == NULL)
        mquit("concat failed to create restore_path");
    if (make_subdirs(restore_path))
        mquit("make_subdirs failed");
    if (cp_file(hash_path, restore_path))
        mquit("cp_file failed");

  clean_up:
    free(hash_path);
    free(restore_path);
    return ret;
}

#define make_store_subdir(res, suffix) do { \
    if ((res = concat(pinfo.store_dir, DIRSEP_STR, suffix, NULL)) == NULL) \
        mquit("concat failed to create res"); \
    if (!is_dir(res) && mkdir(res)) \
        mquit("mkdir failed"); \
} while (0)

int main(int argc, char **argv)
{
    int ret = 0;
    char *operation, *search_dir, *snapshot_name;
    struct process_info pinfo;
    char *ht_path = NULL, *snapshot_dir = NULL, *snapshot_path = NULL;

    pinfo.ht = NULL;
    pinfo.snapshot = NULL;
    pinfo.path_to_files = NULL;
    pinfo.restore_dir = NULL;

    if (argc != 5) {
        fprintf(stderr,
                "Usage: %s -backup search_dir store_dir snapshot_name\n",
                *argv);
        fprintf(stderr,
                "Or: %s -restore store_dir snapshot_name restore_dir\n",
                *argv);
        quit();
    }

    operation = *(argv + 1);

    if (strcmp(operation, "-backup") && strcmp(operation, "-restore"))
        mquit("operation must be -backup or -restore");

    if (!strcmp(operation, "-backup")) {
        /* Backup */
        search_dir = *(argv + 2);
        pinfo.store_dir = *(argv + 3);
        snapshot_name = *(argv + 4);

        if (!is_dir(search_dir))
            mquit("search_dir is not a directory");

        if (!is_dir(pinfo.store_dir))
            mquit("store_dir is not a directory");

        make_store_subdir(snapshot_dir, "snapshots");

        if ((snapshot_path =
             concat(snapshot_dir, DIRSEP_STR, snapshot_name,
                    NULL)) == NULL)
            mquit("concat failed to create snapshot_path");

        if (exists(snapshot_path))
            mquit("snapshot already exists");

        make_store_subdir(pinfo.path_to_files, "files");

        if ((pinfo.ht = init_hashtable(HASH_TABLE_SIZE)) == NULL)
            mquit("init_hashtable failed");

        if ((ht_path =
             concat(pinfo.store_dir, DIRSEP_STR, "ht", NULL)) == NULL)
            mquit("concat failed to create ht_path");

        if (exists(ht_path) && load_file_into_ht(pinfo.ht, ht_path))
            mquit("load_file_into_ht failed");

        if ((pinfo.snapshot = init_buf(INIT_BUF_SIZE)) == NULL)
            mquit("init_buf failed");

        if (walk_dir(search_dir, &pinfo, process_file_capybara))
            mquit("walk_dir failed");

        /* Save hash table */
        if (write_hashtable(pinfo.ht, ht_path))
            mquit("write_hashtable failed");

        /* Save snapshot */
        if (write_buf(pinfo.snapshot, snapshot_path))
            mquit("write_buf failed");

    } else {
        /* Restore */
        pinfo.store_dir = *(argv + 2);
        snapshot_name = *(argv + 3);
        pinfo.restore_dir = *(argv + 4);

        if ((snapshot_path =
             concat(pinfo.store_dir, DIRSEP_STR, "snapshots", DIRSEP_STR,
                    snapshot_name, NULL)) == NULL)
            mquit("concat failed to create snapshot_path");

        if (!exists(snapshot_path))
            mquit("snapshot_path does not exist");

        if (exists(pinfo.restore_dir))
            mquit("restore_dir already exists");

        if (read_pair_file(snapshot_path, &pinfo, process_pair_capybara))
            mquit("read_pair_file failed");
    }

  clean_up:
    free(snapshot_dir);
    free(snapshot_path);
    free(pinfo.path_to_files);
    free(ht_path);

    free_hashtable(pinfo.ht);
    free_buf(pinfo.snapshot);

    return ret;
}
