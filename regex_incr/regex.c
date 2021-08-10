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

#define NUM_CAP_GRP 10

struct atom {
    char set[NUM_UCHAR];        /* Character set */
    char negate;                /* Negate the character set */
    char end;                   /* End of atom sequence */
    ssize_t min_occ;            /* Minimum number of occurrences required */
    ssize_t max_occ;            /* Maximum (as above). -1 means no limit. */
    size_t num;                 /* Number of matches. Default is 1. */
};

/* Capture group */
struct cap_grp {
    ssize_t atom_start;         /* Start index of capture group (inclusive). -1 means not used. */
    ssize_t atom_end;           /* End index of capture group (exclusive). -1 means not used. */
    char *p;                    /* Pointer to start of captured text */
    size_t len;                 /* Length of captured text */
};

#define CR_BAIL do { \
    free(cr); \
    return NULL; \
} while (0)

struct atom *compile_regex(char *find, struct cap_grp *cg)
{
    struct atom *cr;
    unsigned char u;
    size_t i, j, atom_index = 0;
    int in_set = 0;
    size_t cap_grp_index = 0;   /* Capture group index of open bracket */
    size_t stack[NUM_CAP_GRP] = { 0 };  /* Stack for nested capture groups */
    size_t si = 0;              /* Stack index */
    size_t len = strlen(find);

    /* Addition is OK as this size is already in memory */
    if ((cr = calloc(len + 1, sizeof(struct atom))) == NULL)
        return NULL;

    /* Set defaults */
    for (i = 0; i < len + 1; ++i) {
        cr[i].min_occ = 1;
        cr[i].max_occ = 1;
        cr[i].num = 1;
    }

    /* Clear the capture groups */
    for (j = 0; j < NUM_CAP_GRP; ++j) {
        cg[j].atom_start = -1;
        cg[j].atom_end = -1;
        cg[j].p = NULL;
        cg[j].len = 0;
    }

    while ((u = *find++)) {
        switch (u) {
        case '\\':
            /* Escaping. Next char is considered a literal in any context. */
            if (!*find)
                CR_BAIL;
            cr[atom_index].set[(unsigned char) *find] = 'Y';
            if (!in_set)
                ++atom_index;
            /* Eat char */
            ++find;
            break;
        case '(':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else {
                ++cap_grp_index;
                if (cap_grp_index >= NUM_CAP_GRP)
                    CR_BAIL;
                cg[cap_grp_index].atom_start = atom_index;
                stack[si++] = cap_grp_index;
            }
            break;
        case ')':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else {
                if (!cap_grp_index || !si)
                    CR_BAIL;
                cg[stack[--si]].atom_end = atom_index;
            }
            break;
        case '[':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else {
                in_set = 1;
                /* Look ahead for negation operator */
                if (*find == '^') {
                    cr[atom_index].negate = 'Y';
                    /* Eat char */
                    ++find;
                }
            }
            break;
        case ']':
            in_set = 0;
            ++atom_index;
            break;
        case '*':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else {
                if (!atom_index)
                    CR_BAIL;
                cr[atom_index - 1].min_occ = 0;
                cr[atom_index - 1].max_occ = -1;
            }
            break;
        case '+':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else {
                if (!atom_index)
                    CR_BAIL;
                cr[atom_index - 1].min_occ = 1;
                cr[atom_index - 1].max_occ = -1;
            }
            break;
        case '?':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else {
                if (!atom_index)
                    CR_BAIL;
                cr[atom_index - 1].min_occ = 0;
                cr[atom_index - 1].max_occ = 1;
            }
            break;
        default:
            cr[atom_index].set[u] = 'Y';
            if (!in_set)
                ++atom_index;
            break;
        }
    }

    /* Terminate atom array */
    cr[atom_index].end = 'Y';

    /* Checks: */
    /* Unclosed character group */
    if (in_set)
        CR_BAIL;
    /* Unclosed capture group */
    for (j = 0; j < NUM_CAP_GRP; ++j)
        if (cg[j].atom_start != -1 && cg[j].atom_end == -1)
            CR_BAIL;

    /* Set capture group zero */
    cg[0].atom_start = 0;
    cg[0].atom_end = atom_index;

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
        printf(", %ld, %ld, %lu)\n", find->min_occ, find->max_occ,
               find->num);
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
        if (r != NULL) {
            /* Record the number of times that this atom was matched */
            find->num = t - str;
            return r;
        }
        --t;
    }
    /* No match */
    return NULL;
}

void fill_in_capture_groups(struct cap_grp *cg, char *match_p,
                            struct atom *find)
{
    ssize_t i = 0, j, running_total = 0;
    while (!find[i].end) {
        for (j = 0; j < NUM_CAP_GRP; ++j) {
            if (i == cg[j].atom_start)
                cg[j].p = match_p + running_total;
            if (i >= cg[j].atom_start && i < cg[j].atom_end)
                cg[j].len += find[i].num;
        }
        running_total += find[i].num;
        ++i;
    }
}

void print_capture_groups(struct cap_grp *cg)
{
    size_t j;
    for (j = 0; j < NUM_CAP_GRP; ++j) {
        printf("Capture group %lu (%ld, %ld): ", j, cg[j].atom_start,
               cg[j].atom_end);
        cg[j].p == NULL ? printf("NULL") : fwrite(cg[j].p, 1, cg[j].len,
                                                  stdout);
        putchar('\n');
    }
}

int main(void)
{
    struct atom *cr;
    struct cap_grp cg[NUM_CAP_GRP];
    char *find = "(([^x*[(y]+)b)(c)";
    char *str = "xxxaaaaaaaaaaaefgjbcuuu";
    char *p;
    size_t len;

    if ((cr = compile_regex(find, cg)) == NULL)
        return 1;

    print_compiled_regex(cr);


    /* printf("Match: %c\n", match_regex_here(cr, str) == NULL ? 'N' : 'Y'); */


    if ((p = match_regex(cr, str, &len)) == NULL)
        return 1;

    fwrite(p, 1, len, stdout);
    putchar('\n');


    print_compiled_regex(cr);

    fill_in_capture_groups(cg, p, cr);


    print_capture_groups(cg);


    free(cr);

    return 0;
}
