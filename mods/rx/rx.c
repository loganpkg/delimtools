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

#ifdef RR_DEBUG
#include <stdio.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../gen/gen.h"
#include "../regex/regex.h"
#include "rx.h"

struct buf *regex_replace(char *find, char *replace, char *str, int nl_sen)
{
    int ret = 0, r = 0;
    regex_t cr;                 /* Compiled regex */
    regmatch_t *mat = NULL;     /* One lot of matches with capturing groups */
    size_t init_buf_size, i, num, cg_s, left_over;
    int cr_used = 0;            /* Compiled regex struct has been used */
    int sol = 1;                /* Start of str is start of line */
    struct buf *result = NULL;
    char *rep, ch;

    if (find == NULL || replace == NULL || str == NULL)
        return NULL;

    /* Determine a sensible initial size for the result buffer */
    init_buf_size = strlen(str) + 1;
    if (mof(init_buf_size, 2))
        return NULL;
    init_buf_size *= 2;

    if ((result = init_buf(init_buf_size)) == NULL)
        quit();

    if (regcomp(&cr, find, REG_EXTENDED | (nl_sen ? REG_NEWLINE : 0)))
        quit();

    cr_used = 1;

    num = cr.re_nsub;

    /* Increase num to count the \0 capturing group (the whole match) */
    if (aof(num, 1))
        quit();
    ++num;

    if ((mat = calloc(num, sizeof(regmatch_t))) == NULL)
        quit();

    while (*str != '\0'
           && !(r = regexec(&cr, str, num, mat, sol ? 0 : REG_NOTBOL))) {
        sol = 0;

#ifdef RR_DEBUG
        printf("(%lld,%lld)", mat[0].rm_eo, mat[0].rm_so);
        fwrite(str, 1, mat[0].rm_eo - mat[0].rm_so, stdout);
        putchar('\n');
#endif

        /* Copy text before match */
        if (put_mem(result, str, mat[0].rm_so))
            quit();

        /* Copy and process replacement text */
        rep = replace;
        while ((ch = *rep++)) {
            if (ch == '\\' && isdigit(*rep)) {
                /* Backreference */
                i = *rep - '0';
                ++rep;          /* Eat the digit */
                /*
                 * Backreference exceeds the use of capturing groups in the
                 * orginal find regex.
                 */
                if (i >= num)
                    quit();
                cg_s = mat[i].rm_eo - mat[i].rm_so;
                if (cg_s && put_mem(result, str + mat[i].rm_so, cg_s))
                    quit();
            } else {
                if (put_ch(result, ch))
                    quit();
            }
        }

        /* Move forward to the end of the match */
        str += mat[0].rm_eo;
        /* Move forward one more if a zero size match */
        if (!(mat[0].rm_eo - mat[0].rm_so) && *str != '\0') {
            ch = *str;
            /*
             * Turn on start of line indicator if moving into a new line
             * (and in newline sensitive mode).
             */
            if (nl_sen && ch == '\n')
                sol = 1;
            if (put_ch(result, ch))
                quit();
            ++str;
        }
    }

    if (r && r != REG_NOMATCH)
        quit();

    /* Copy text after last match (or all the text if there was no match) */
    left_over = strlen(str);
    put_mem(result, str, left_over);

    /* Terminate, so that the buffer can be used as a string */
    if (put_ch(result, '\0'))
        quit();

  clean_up:
    if (cr_used)
        regfree(&cr);

    if (mat != NULL)
        free(mat);

    if (ret && result != NULL) {
        free_buf(result);
        return NULL;
    }

    return result;
}
