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

/* Regular expression module */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define REGEX_DBUG

#define NUM_UCHAR (UCHAR_MAX + 1)

#define match_atom(atom, ch) (atom->set[(unsigned char) ch] != atom->negate)


struct atom {
    char set[NUM_UCHAR];        /* Character set */
    char negate;                /* Negate the character set */
    char end;                   /* End of atom sequence */
    ssize_t min_occ;            /* Minimum number of occurrences required */
    ssize_t max_occ;            /* Maximum (as above). -1 means no limit. */
};

struct atom *compile_regex(char *find)
{
    struct atom *cr, *t;
    unsigned char u;
    size_t len = strlen(find);
    /* Addition is OK as this size is already in memory */
    if ((cr = calloc(len + 1, sizeof(struct atom))) == NULL)
        return NULL;
    t = cr;
    while ((u = *find++)) {
        t->set[u] = 'Y';
        t->min_occ = 1;
        t->max_occ = 1;
        ++t;
    }
    t->end = 'Y';
    return cr;
}

void print_compiled_regex(struct atom *find)
{
    size_t i;
    while (!find->end) {
        putchar('(');
        for (i = 0; i < NUM_UCHAR; ++i)
            if (find->set[i])
                putchar(i);
        printf(", %ld, %ld)\n", find->min_occ, find->max_occ);
        ++find;
    }
}

char *match_regex_here(struct atom *find, char *str);

char *match_regex(struct atom *find, char *str, size_t * len)
{
    char *end_p;
    do {
        printf("match_regex: str: %s\n", str);
        end_p = match_regex_here(find, str);
    } while (end_p == NULL && *str++ != '\0');

    if (end_p == NULL)
        return NULL;

    *len = end_p - str;         /* Length of match */
    return str;
}

char *match_regex_mult(struct atom *find, char *str);
char *match_regex_here(struct atom *find, char *str)
{
    /* print_compiled_regex(find); */
    printf("match_regex_here: str: %s\n", str);

    /* Match */
    if (find->end)
        return str;
    if (find->min_occ == 1 && find->max_occ == 1
        && match_atom(find, *str) && *str != '\0')
        return match_regex_here(++find, ++str);
    if (!(find->min_occ == 1 && find->max_occ == 1) && *str != '\0')
        return match_regex_mult(find, str);

    /* No match */
    return NULL;
}



char *match_regex_mult(struct atom *find, char *str)
{
    char *t = str, *r;
    printf("match_regex_mult: str: %s\n", str);
    /* Find the most repeats that possible */
    while (match_atom(find, *t)
           && (find->max_occ == -1 ? 1 : t - str < find->max_occ)
           && *t != '\0')
        ++t;
    /* Too few matches */
    if (t - str < find->min_occ)
        return NULL;
    /* Work backwards to see if the rest of the pattern will match */
    while (t - str >= find->min_occ) {
        r = match_regex_here(find + 1, t);
        if (r != NULL)
            return r;
        --t;
    }
    /* No match */
    return NULL;
}

int main(void)
{
    struct atom *cr;
    char *find = "abc";
    char *str = "xxxaaaaaaaaaaaefgbcuuu";
    char *p;
    size_t len;

    if ((cr = compile_regex(find)) == NULL)
        return 1;

    /* print_compiled_regex(cr); */

    /* printf("Match: %c\n", match_regex_here(cr, str) == NULL ? 'N' : 'Y'); */


    cr[0].min_occ = 1;
    cr[0].max_occ = -1;
    cr->negate = 'Y';


    if ((p = match_regex(cr, str, &len)) == NULL)
        return 1;

    fwrite(p, 1, len, stdout);
    putchar('\n');

    free(cr);

    return 0;
}
