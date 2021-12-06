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

/* Random functions */

#ifdef _WIN32
#define _CRT_RAND_S
#endif
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "../gen/gen.h"
#include "random.h"

char *random_alnum_str(size_t len)
{
    /*
     * Returns a random alphanumeric string of length len.
     * Must free after use. Returns NULL upon failure.
     */
    char *p;
    unsigned char *t, u;
    size_t i, j;
#ifdef _WIN32
    unsigned int r;
    size_t ts = sizeof(unsigned int);
#else
    FILE *fp;
    size_t ts = 64;
#endif

    if (aof(len, 1))
        return NULL;
    if ((p = malloc(len + 1)) == NULL)
        return NULL;

#ifndef _WIN32
    if ((t = malloc(ts)) == NULL) {
        free(p);
        return NULL;
    }
    if ((fp = fopen("/dev/urandom", "r")) == NULL) {
        free(p);
        free(t);
        return NULL;
    }
#endif

    j = 0;
    while (j != len) {
#ifdef _WIN32
        if (rand_s(&r)) {
            free(p);
            return NULL;
        }
        t = (unsigned char *) &r;
#else
        if (fread(t, 1, ts, fp) != ts) {
            free(p);
            free(t);
            fclose(fp);
            return NULL;
        }
#endif

        for (i = 0; i < ts; ++i) {
            u = *(t + i);
            if (isalnum(u)) {
                *(p + j++) = u;
                if (j == len)
                    break;
            }
        }
    }

    *(p + j) = '\0';

#ifndef _WIN32
    free(t);
    if (fclose(fp)) {
        free(p);
        return NULL;
    }
#endif

    return p;
}
