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

/*
 * References:
 *
 * 9.2 Regular Expressions:
 * Brian W. Kernighan and Rob Pike, The Practice of Programming,
 *     Addison Wesley Longman, Massachusetts, 1999.
 *
 * Brian Kernighan, Beautiful Code: Chapter 1: A Regular Expression Matcher,
 *     O'Reilly Media, California, 2007.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "../gen/gen.h"
#include "regex.h"

/* #define REGEX_DBUG */

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

struct hook {
    char start;                 /* ^ */
    char end;                   /* $ */
};

/*
 * Capture group.
 * An atom_start or atom_end value of -1 signifys that it is not used.
 */
struct cap_grp {
    ssize_t atom_start;         /* Start index of capture group (inclusive) */
    ssize_t atom_end;           /* End index of capture group (exclusive) */
    char *p;                    /* Pointer to start of captured text */
    size_t len;                 /* Length of captured text */
};

#define cr_bail() do { \
    free(cr); \
    return NULL; \
} while (0)


static struct atom *compile_regex(char *find, struct cap_grp *cg,
                                  struct hook *hk);
static char *match_regex(struct atom *find, struct hook *hk, char *str,
                         int sol, size_t * len);
static char *match_regex_here(struct atom *find, struct hook *hk,
                              char *str);
static char *match_regex_mult(struct atom *find, struct hook *hk,
                              char *str);
static void fill_in_capture_groups(struct cap_grp *cg, char *match_p,
                                   struct atom *find);

#ifdef REGEX_DBUG
static void print_compiled_regex(struct atom *find);
static void print_hooks(struct hook *hk);
static void print_capture_groups(struct cap_grp *cg);
#endif


static struct atom *compile_regex(char *find, struct cap_grp *cg,
                                  struct hook *hk)
{
    struct atom *cr;
    unsigned char u, e;
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

    /* Clear hooks */
    hk->start = '\0';
    hk->end = '\0';

    /* Check for ^ hook */
    if (*find == '^') {
        hk->start = 'Y';
        /* Eat char */
        ++find;
    }

    while ((u = *find++)) {
        switch (u) {
        case '\\':
            /* Escaping. Next char is considered a literal in any context. */
            if (!*find)
                cr_bail();
            cr[atom_index].set[(unsigned char) *find] = 'Y';
            if (!in_set)
                ++atom_index;
            /* Eat char */
            ++find;
            break;
        case '$':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else if (!*find) {
                /* Hook */
                hk->end = 'Y';
            } else {
                /* Just a stand-alone char */
                cr[atom_index].set[u] = 'Y';
                ++atom_index;
            }
            break;
        case '(':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else {
                ++cap_grp_index;
                if (cap_grp_index >= NUM_CAP_GRP)
                    cr_bail();
                cg[cap_grp_index].atom_start = atom_index;
                stack[si++] = cap_grp_index;
            }
            break;
        case ')':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else {
                if (!cap_grp_index || !si)
                    cr_bail();
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
                    cr_bail();
                cr[atom_index - 1].min_occ = 0;
                cr[atom_index - 1].max_occ = -1;
            }
            break;
        case '+':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else {
                if (!atom_index)
                    cr_bail();
                cr[atom_index - 1].min_occ = 1;
                cr[atom_index - 1].max_occ = -1;
            }
            break;
        case '?':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else {
                if (!atom_index)
                    cr_bail();
                cr[atom_index - 1].min_occ = 0;
                cr[atom_index - 1].max_occ = 1;
            }
            break;
        case '.':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else {
                /* All possible chars */
                for (i = 0; i < NUM_UCHAR; ++i)
                    cr[atom_index].set[i] = 'Y';
                ++atom_index;
            }
            break;
        default:
            if (in_set) {
                /* Look ahead for range */
                if (*find == '-' && (e = *(find + 1))) {
                    if (e < u)
                        cr_bail();
                    for (i = u; i <= e; ++i)
                        cr[atom_index].set[i] = 'Y';
                    /* Eat chars */
                    find += 2;
                } else {
                    /* Just add the char to the set */
                    cr[atom_index].set[u] = 'Y';
                }
            } else {
                cr[atom_index].set[u] = 'Y';
                ++atom_index;
            }
            break;
        }
    }

    /* Terminate atom array */
    cr[atom_index].end = 'Y';

    /* Checks: */
    /* Unclosed character group */
    if (in_set)
        cr_bail();
    /* Unclosed capture group */
    for (j = 0; j < NUM_CAP_GRP; ++j)
        if (cg[j].atom_start != -1 && cg[j].atom_end == -1)
            cr_bail();

    /* Set capture group zero */
    cg[0].atom_start = 0;
    cg[0].atom_end = atom_index;

    return cr;
}

static char *match_regex(struct atom *find, struct hook *hk, char *str,
                         int sol, size_t * len)
{
    char *end_p;

    /*
     * Short-circuit if there is a start of line hook ^ and the string does not
     * begin at the start of the line.
     */
    if (hk->start && !sol)
        return NULL;

    /* End of line hook $ by itself */
    if (hk->end && find->end) {
        *len = 0;
        return str + strlen(str);
    }

    do {
#ifdef REGEX_DBUG
        printf("match_regex     : str: %s\n", str);
#endif
        end_p = match_regex_here(find, hk, str);
        /* Keep searching if there is no start hook ^ and no match */
    } while (!hk->start && end_p == NULL && *str++ != '\0');

    if (end_p == NULL)
        return NULL;

    *len = end_p - str;         /* Length of match */
    return str;
}

static char *match_regex_here(struct atom *find, struct hook *hk,
                              char *str)
{
#ifdef REGEX_DBUG
    printf("match_regex_here: str: %s\n", str);
#endif
    /* End of regex atom chain */
    if (find->end) {
        /* Failed end hook */
        if (hk->end && *str)
            return NULL;
        /* Match */
        return str;
    }
    if (find->min_occ == 1 && find->max_occ == 1
        && match_atom(find, *str) && *str != '\0')
        return match_regex_here(++find, hk, ++str);
    if (!(find->min_occ == 1 && find->max_occ == 1) && *str != '\0')
        return match_regex_mult(find, hk, str);

    /* No match */
    return NULL;
}

static char *match_regex_mult(struct atom *find, struct hook *hk,
                              char *str)
{
    char *t = str, *r;
#ifdef REGEX_DBUG
    printf("match_regex_mult: str: %s\n", str);
#endif
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
        r = match_regex_here(find + 1, hk, t);
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

static void fill_in_capture_groups(struct cap_grp *cg, char *match_p,
                                   struct atom *find)
{
    ssize_t i = 0, j, running_total = 0;
    /* Clear values */
    for (j = 0; j < NUM_CAP_GRP; ++j) {
        cg[j].p = NULL;
        cg[j].len = 0;
    }

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

    /* Set length on capture group zero */
    cg[0].len = running_total;
}

struct buf *regex_replace(char *find, char *replace, char *str, int nl_sen)
{
    /* Regular expression find and replace */

    int ret = 0;
    char *t = NULL, *line, *q = NULL;
    char *p;                    /* Pointer to regex match */
    size_t len;                 /* Length of regex match */
    char *rep, ch;
    size_t j;
    struct buf *result = NULL;
    size_t init_buf_size = BUFSIZ;
    struct atom *cr = NULL;
    struct cap_grp cg[NUM_CAP_GRP];
    struct hook hk;
    int sol = 1;                /* Start of line indicator */

    /* Compile regex expression */
    if ((cr = compile_regex(find, cg, &hk)) == NULL)
        return NULL;

#ifdef REGEX_DBUG
    print_compiled_regex(cr);
    print_hooks(&hk);
#endif

    /*
     * Copy string if in newline sensitive mode as the \n chars will be
     * replaced with \0.
     */
    if (nl_sen) {
        if ((t = strdup(str)) == NULL)
            quit();
        else
            str = t;
    }

    if ((result = init_buf(init_buf_size)) == NULL)
        quit();

    line = str;

    /* Process line by line */
    do {
        /* Terminate line */
        if (nl_sen) {
            if ((q = strchr(line, '\n')) != NULL)
                *q = '\0';
        }
        while (1) {
            /* Match regex as many times as possible on line */
            if ((p = match_regex(cr, &hk, line, sol, &len)) == NULL)
                break;
            /* Turn off as there can be multiple matches on the same line */
            sol = 0;
            /* Fill in caputure group matches */
            fill_in_capture_groups(cg, p, cr);

#ifdef REGEX_DBUG
            print_capture_groups(cg);
            printf("%s\n%lu\n", line, strlen(line));
#endif

            /* Copy text before the match */
            if (put_mem(result, line, p - line))
                quit();
            /* Copy replacement text */
            rep = replace;
            while ((ch = *rep++)) {
                if (ch == '\\' && isdigit(*rep)) {
                    /* Substitute backreferences to capture groups */
                    j = *rep - '0';
                    ++rep;      /* Eat the digit */
                    /*
                     * Backreference exceeds number of capture groups in the
                     * orginal find regex.
                     */
                    if (cg[j].atom_start == -1)
                        quit();
                    if (cg[j].len && put_mem(result, cg[j].p, cg[j].len))
                        quit();
                } else {
                    if (put_ch(result, ch))
                        quit();
                }
            }
            /*
             * Move forward on the same line to after the end of the match.
             * Don't need to worry about zero length matches here, as can only
             * happen with the ^ and $ hooks by themselves in this regex
             * engine.
             */
            line = p + len;
            /* Break if at end of line */
            if (!*line)
                break;
        }
        /* Copy the rest of the line */
        if (put_str(result, line))
            quit();
        /* Replace newline character */
        if (nl_sen && q != NULL && put_ch(result, '\n'))
            quit();

        sol = 1;
        /* Move to the next line if doing newline sensitive matching */
        if (nl_sen && q != NULL)
            line = q + 1;
    } while (nl_sen && q != NULL);

    /* Terminate buf in case it is used as a string */
    if (put_ch(result, '\0'))
        quit();

  clean_up:
    free(cr);
    free(t);

    if (ret) {
        free_buf(result);
        return NULL;
    }

    return result;
}

#ifdef REGEX_DBUG

static void print_compiled_regex(struct atom *find)
{
    size_t i;
    while (!find->end) {
        putchar('(');
        for (i = 0; i < NUM_UCHAR; ++i)
            if (find->set[i]) {
                if (isgraph(i))
                    putchar(i);
                else
                    printf("%02X", (unsigned char) i);
            }
        printf(", %ld, %ld, %lu)\n", find->min_occ, find->max_occ,
               find->num);
        ++find;
    }
}

static void print_hooks(struct hook *hk)
{
    printf("Start hook (^): %c\n", hk->start ? 'Y' : 'N');
    printf("End hook   ($): %c\n", hk->end ? 'Y' : 'N');
}

static void print_capture_groups(struct cap_grp *cg)
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
    char *str = "hello world\n" "world hello";
    char *find = "world";
    char *replace = "\\0\\0";
    struct buf *r;

    printf("========\n%s\n========\n%s\n========\n%s\n", find, replace,
           str);
    if ((r = regex_replace(find, replace, str, 1)) == NULL)
        return 1;
    printf("========\n%s\n========\n", r->a);
    free_buf(r);

    return 0;
}

#endif
