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


#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <limits.h>
#include <ctype.h>

#include "gen.h"

int str_to_num(char *s, size_t * num)
{
    char ch;
    size_t n = 0, len = 0;
    if (s == NULL)
        return 1;
    while ((ch = *s++)) {
        ++len;
        if (isdigit(ch)) {
            if (mof(n, 10))
                return 1;
            n *= 10;
            if (aof(n, (ch - '0')))
                return 1;
            n += (ch - '0');
        } else {
            return 1;
        }
    }
    if (!len)
        return 1;
    *num = n;
    return 0;
}

char *memmatch(char *big, size_t big_len, char *little, size_t little_len)
{
    /*
     * Quick Search algorithm:
     * Daniel M. Sunday, A Very Fast Substring Search Algorithm,
     *     Communications of the ACM, Vol.33, No.8, August 1990.
     */
#define UCHAR_NUM (UCHAR_MAX + 1)
    unsigned char bad[UCHAR_NUM], *pattern, *q, *q_stop, *q_check;
    size_t to_match, j;
    if (!little_len)
        return big;
    if (little_len > big_len)
        return NULL;
    for (j = 0; j < UCHAR_NUM; ++j)
        bad[j] = little_len + 1;
    pattern = (unsigned char *) little;
    for (j = 0; j < little_len; ++j)
        bad[pattern[j]] = little_len - j;
    q = (unsigned char *) big;
    q_stop = (unsigned char *) big + big_len - little_len;
    while (q <= q_stop) {
        q_check = q;
        pattern = (unsigned char *) little;
        to_match = little_len;
        /* Compare pattern to text */
        while (to_match && *q_check++ == *pattern++)
            --to_match;
        /* Match found */
        if (!to_match)
            return (char *) q;
        /* Jump using the bad character table */
        q += bad[q[little_len]];
    }
    /* Not found */
    return NULL;
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