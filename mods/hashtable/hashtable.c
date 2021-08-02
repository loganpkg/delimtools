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

/*
 * References:
 * Section 6.6 Table Lookup of:
 * Brian W. Kernighan and Dennis M. Ritchie, The C Programming Language,
 *     Second Edition, Prentice Hall Software Series, New Jersey, 1988.
 * Clovis L. Tondo and Scott E. Gimpel, The C Answer Book, Second Edition,
 *     PTR Prentice Hall Software Series, New Jersey, 1989.
 * Daniel J. Bernstein's djb2 algorithm from:
 * Hash Functions, http://www.cse.yorku.ca/~oz/hash.html
 */

#ifdef __linux__
/* For strdup */
#define _XOPEN_SOURCE 500
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashtable.h"

struct hashtable *init_hashtable(size_t num_buckets)
{
    struct hashtable *ht;
    if ((ht = calloc(1, sizeof(struct hashtable))) == NULL)
        return NULL;
    if ((ht->b = calloc(num_buckets, sizeof(struct entry *))) == NULL) {
        free(ht);
        return NULL;
    }
    ht->n = num_buckets;
    return ht;
}

void free_hashtable(struct hashtable *ht)
{
    struct entry *e, *ne;
    size_t j;
    if (ht != NULL) {
        if (ht->b != NULL) {
            for (j = 0; j < ht->n; ++j) {
                e = *(ht->b + j);
                while (e != NULL) {
                    ne = e->next;
                    free(e->name);
                    free(e->def);
                    free(e);
                    e = ne;
                }
            }
            free(ht->b);
        }
        free(ht);
    }
}

static struct entry *init_entry(void)
{
    struct entry *e;
    if ((e = malloc(sizeof(struct entry))) == NULL)
        return NULL;
    e->next = NULL;
    e->name = NULL;
    e->def = NULL;
    return e;
}

static size_t hash_str(struct hashtable *ht, char *s)
{
    /* djb2 */
    unsigned char c;
    size_t h = 5381;
    while ((c = *s++))
        h = h * 33 ^ c;
    return h % ht->n;
}

void htdist(struct hashtable *ht)
{
    struct entry *e;
    size_t freq[101] = { 0 }, count, k;
    for (k = 0; k < ht->n; ++k) {
        e = *(ht->b + k);
        count = 0;
        while (e != NULL) {
            e = e->next;
            ++count;
        }
        count < 100 ? ++*(freq + count) : ++*(freq + 100);
    }
    fprintf(stderr, "entries_per_bucket number_of_buckets\n");
    for (k = 0; k < 100; k++)
        if (*(freq + k))
            fprintf(stderr, "%lu %lu\n", (unsigned long) k,
                    (unsigned long) *(freq + k));
    if (*(freq + 100))
        fprintf(stderr, ">=100 %lu\n", (unsigned long) *(freq + 100));
}

struct entry *lookup_entry(struct hashtable *ht, char *name)
{
    size_t h = hash_str(ht, name);
    struct entry *e = *(ht->b + h);
    while (e != NULL) {
        /* Found */
        if (!strcmp(name, e->name))
            return e;
        e = e->next;
    }
    /* Not found */
    return NULL;
}

char *get_def(struct hashtable *ht, char *name)
{
    struct entry *e;
    if ((e = lookup_entry(ht, name)) == NULL)
        return NULL;
    return e->def;
}

int upsert_entry(struct hashtable *ht, char *name, char *def)
{
    struct entry *e;
    size_t h;
    char *t = NULL;
    if ((e = lookup_entry(ht, name)) == NULL) {
        /* Insert entry: */
        if ((e = init_entry()) == NULL)
            return 1;
        /* Store data */
        if ((e->name = strdup(name)) == NULL) {
            free(e);
            return 1;
        }
        if (def != NULL && (e->def = strdup(def)) == NULL) {
            free(e->name);
            free(e);
            return 1;
        }
        h = hash_str(ht, name);
        /* Link new entry in at head of list (existing head could be NULL) */
        e->next = *(ht->b + h);
        *(ht->b + h) = e;
    } else {
        /* Update entry: */
        if (def != NULL && (t = strdup(def)) == NULL)
            return 1;
        free(e->def);
        e->def = t;             /* Could be NULL */
    }
    return 0;
}

int delete_entry(struct hashtable *ht, char *name)
{
    size_t h = hash_str(ht, name);
    struct entry *e = *(ht->b + h), *prev = NULL;
    while (e != NULL) {
        /* Found */
        if (!strcmp(name, e->name))
            break;
        prev = e;
        e = e->next;
    }
    if (e != NULL) {
        /* Found */
        /* Link around the entry */
        if (prev != NULL)
            prev->next = e->next;
        free(e->name);
        free(e->def);
        free(e);
        /* At head of list */
        if (prev == NULL)
            *(ht->b + h) = NULL;
        return 0;
    }

    /* Not found */
    return 1;
}
