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

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

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
    char *path_files;
};

int process_file_capybara(char *fn, void *info)
{
    int ret = 0;
    char *hash = NULL, *path_hash = NULL;
    struct process_info *p = info;

    if ((hash = sha256(fn)) == NULL)
        quit();

    /* Check to see if it is in hash table already */
    if (lookup_entry(p->ht, hash) == NULL) {
        /* Prepare destination path */
        if ((path_hash =
             concat(p->path_files, DIRSEP_STR, hash, NULL)) == NULL)
            quit();
        /* Copy file */
        if (cp_file(fn, path_hash))
            quit();
        /* Add entry to hash table */
        if (upsert_entry(p->ht, hash, NULL))
            quit();
        /* Add filename mapping to snapshot list */
        if (put_str(p->snapshot, fn))
            quit();
        if (put_ch(p->snapshot, '\0'))
            quit();
        if (put_str(p->snapshot, path_hash))
            quit();
        if (put_ch(p->snapshot, '\0'))
            quit();
    }

  clean_up:
    free(hash);
    free(path_hash);
    return ret;
}

#define make_store_subdir(res, suffix) do { \
    if ((res = concat(pinfo.store_dir, DIRSEP_STR, suffix, NULL)) == NULL) \
        quit(); \
    if (!exists(res) && mkdir(res, 0700)) \
        quit(); \
    if (!is_dir(res)) \
        quit(); \
} while (0)

int main(int argc, char **argv)
{
    int ret = 0;
    char *search_dir, *snapshot_name;
    struct process_info pinfo;
    char *ht_path = NULL, *snapshot_dir = NULL, *snapshot_path = NULL;

    pinfo.ht = NULL;
    pinfo.snapshot = NULL;
    pinfo.path_files = NULL;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s search_dir store_dir snapshot_name\n",
                *argv);
        quit();
    }

    search_dir = *(argv + 1);
    pinfo.store_dir = *(argv + 2);
    snapshot_name = *(argv + 3);

    if (!is_dir(search_dir) || !is_dir(pinfo.store_dir))
        quit();

    make_store_subdir(snapshot_dir, "snapshots");

    if ((snapshot_path =
         concat(snapshot_dir, DIRSEP_STR, snapshot_name, NULL)) == NULL)
        quit();

    if (exists(snapshot_path)) {
        fprintf(stderr, "%s: Error: snapshot already exists\n", *argv);
        quit();
    }

    make_store_subdir(pinfo.path_files, "files");

    if ((pinfo.ht = init_hashtable(HASH_TABLE_SIZE)) == NULL)
        quit();

    if ((ht_path =
         concat(pinfo.store_dir, DIRSEP_STR, "ht", NULL)) == NULL)
        quit();

    if (exists(ht_path) && load_file_into_ht(pinfo.ht, ht_path))
        quit();

    if ((pinfo.snapshot = init_buf(INIT_BUF_SIZE)) == NULL)
        quit();

    if (walk_dir(search_dir, &pinfo, process_file_capybara))
        quit();

    /* Save hash table */
    if (write_hashtable(pinfo.ht, ht_path))
        quit();

    /* Save snapshot */
    if (write_buf(pinfo.snapshot, snapshot_path))
        quit();

  clean_up:
    free(snapshot_dir);
    free(snapshot_path);
    free(pinfo.path_files);
    free(ht_path);

    free_hashtable(pinfo.ht);
    free_buf(pinfo.snapshot);

    return ret;
}
