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

#include "../sane_ftm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "../gen/gen.h"
#include "../buf/buf.h"
#include "regex.h"

#define match_atom(atom, ch) (atom->set[(unsigned char) ch] != atom->negate)

#define NUM_CAP_GRP 10

struct atom {
    char set[UCHAR_NUM];        /* Character set */
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


static struct atom *compile_regex(char *find, struct cap_grp *cg,
                                  struct hook *hk)
{
    struct atom *cr;
    unsigned char u, w, e;
    size_t i, j, k, n, atom_index = 0;
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
            if (!*find)
                cr_bail();
            switch (*find) {
                /* Process special character sets first */
            case 'w':
                /* Word set */
                for (k = 0; k < ASCII_NUM; ++k)
                    if (isalnum(k) || k == '_')
                        cr[atom_index].set[k] = 'Y';
                break;
            case 'W':
                /* Non-word set */
                for (k = 0; k < ASCII_NUM; ++k)
                    if (!(isalnum(k) || k == '_'))
                        cr[atom_index].set[k] = 'Y';
                break;
            case 'd':
                /* Digit set */
                for (k = 0; k < ASCII_NUM; ++k)
                    if (isdigit(k))
                        cr[atom_index].set[k] = 'Y';
                break;
            case 'D':
                /* Non-digit set */
                for (k = 0; k < ASCII_NUM; ++k)
                    if (!isdigit(k))
                        cr[atom_index].set[k] = 'Y';
                break;
            case 's':
                /* Whitespace set */
                for (k = 0; k < ASCII_NUM; ++k)
                    if (isspace(k))
                        cr[atom_index].set[k] = 'Y';
                break;
            case 'S':
                /* Non-whitespace set */
                for (k = 0; k < ASCII_NUM; ++k)
                    if (!isspace(k))
                        cr[atom_index].set[k] = 'Y';
                break;
            default:
                /* Escaping, the next char is considered a literal */
                cr[atom_index].set[(unsigned char) *find] = 'Y';
                break;
            }
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
        case '{':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else {
                /* Range */
                if (!atom_index)
                    cr_bail();

                /* Read first number (if any) */
                n = 0;
                while (isdigit(w = *find++)) {
                    if (mof(n, 10))
                        cr_bail();
                    n *= 10;
                    if (aof(n, w - '0'))
                        cr_bail();
                    n += w - '0';
                }
                /* Zero is the default here if no first number was given */
                cr[atom_index - 1].min_occ = n;

                if (w == '}') {
                    /*
                     * Only one number or empty braces. Cannot be zero here.
                     * Set max_occ to the same number.
                     */
                    if (!n)
                        cr_bail();
                    cr[atom_index - 1].max_occ = n;
                    break;      /* Done reading range */
                } else if (w != ',') {
                    /* Syntax error */
                    cr_bail();
                }

                /* Read second number (if any) */
                /* Deafult is -1 here if no second number is given */
                cr[atom_index - 1].max_occ = -1;
                n = 0;
                while (isdigit(w = *find++)) {
                    if (mof(n, 10))
                        cr_bail();
                    n *= 10;
                    if (aof(n, w - '0'))
                        cr_bail();
                    n += w - '0';
                    /* Update number */
                    cr[atom_index - 1].max_occ = n;
                }
                /* The second number cannot be zero */
                if (!cr[atom_index - 1].max_occ)
                    cr_bail();

                /* End range */
                if (w != '}')
                    cr_bail();
            }
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
                for (k = 0; k < UCHAR_NUM; ++k)
                    cr[atom_index].set[k] = 'Y';
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
    /* End of regex atom chain */
    if (find->end) {
        /* Failed end hook */
        if (hk->end && *str)
            return NULL;
        /* Match */
        return str;
    }
    if (find->min_occ == 1 && find->max_occ == 1
        && *str != '\0' && match_atom(find, *str))
        return match_regex_here(++find, hk, ++str);
    if (!(find->min_occ == 1 && find->max_occ == 1))
        return match_regex_mult(find, hk, str);

    /* No match */
    return NULL;
}

static char *match_regex_mult(struct atom *find, struct hook *hk,
                              char *str)
{
    char *t = str, *r;
    /* Find the most repeats possible */
    while (*t != '\0' && match_atom(find, *t)
           && (find->max_occ == -1 ? 1 : t - str < find->max_occ))
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

char *regex_replace(char *str, char *find, char *replace, int nl_insen)
{
    /*
     * Regular expression find and replace. Returns NULL upon failure.
     * Must free the returned string after success.
     */
    int ret = 0;
    char *t = NULL, *line, *q = NULL;
    char *p;                    /* Pointer to regex match */
    size_t len = 0;             /* Length of regex match */
    size_t prev_len = 0;        /* Length of previous regex match */
    char *rep, ch;
    size_t j;
    struct buf *result = NULL;
    size_t init_buf_size = BUFSIZ;
    char *res_str = NULL;
    struct atom *cr = NULL;
    struct cap_grp cg[NUM_CAP_GRP];
    struct hook hk;
    int sol = 1;                /* Start of line indicator */

    /* Compile regex expression */
    if ((cr = compile_regex(find, cg, &hk)) == NULL)
        return NULL;

    /*
     * Copy string if in newline sensitive mode as the \n chars will be
     * replaced with \0.
     */
    if (!nl_insen) {
        if ((t = strdup(str)) == NULL)
            quit();
        else
            str = t;
    }

    if ((result = init_buf(init_buf_size)) == NULL)
        quit();

    /*
     * Do not process an empty string (but an in-the-middle line can be empty).
     * Not an error.
     */
    if (!*str)
        goto clean_up;

    line = str;

    /* Process line by line */
    do {
        /* Terminate line */
        if (!nl_insen) {
            if ((q = strchr(line, '\n')) != NULL)
                *q = '\0';
            /* Do not process the last line if empty. Not an error. */
            else if (!*line)
                goto clean_up;
        }

        while (1) {
            /* Match regex as many times as possible on line */
            if ((p = match_regex(cr, &hk, line, sol, &len)) == NULL)
                break;

            /* Turn off as there can be multiple matches on the same line */
            sol = 0;
            /* Fill in caputure group matches */
            fill_in_capture_groups(cg, p, cr);

            /* Copy text before the match  */
            if (put_mem(result, line, p - line))
                quit();

            /*
             * Skip putting the replacement text if a zero length match and the
             * previous match was not a zero length match. This stops two
             * replacements being put at the same location. Only has scope
             * within a line.
             */
            if (!(!len && prev_len && p == line)) {
                /* Copy replacement text */
                rep = replace;
                while ((ch = *rep++)) {
                    if (ch == '\\' && isdigit(*rep)) {
                        /* Substitute backreferences to capture groups */
                        j = *rep - '0';
                        ++rep;  /* Eat the digit */
                        /*
                         * Backreference exceeds number of capture groups in the
                         * orginal find regex.
                         */
                        if (cg[j].atom_start == -1)
                            quit();
                        if (cg[j].len
                            && put_mem(result, cg[j].p, cg[j].len))
                            quit();
                    } else {
                        if (put_ch(result, ch))
                            quit();
                    }
                }
            }

            /* Record prev len for the next iteration */
            prev_len = len;

            /* Move forward on the same line to after the end of the match */
            line = p + len;
            /* Break if at end of line */
            if (!*line)
                break;
            /*
             * Move forward by one if a zero length match, and pass the skipped
             * character through.
             */
            if (!len && put_ch(result, *line++))
                quit();
        }

        /* Copy the rest of the line */
        if (put_str(result, line))
            quit();
        /* Replace the newline character */
        if (!nl_insen && q != NULL && put_ch(result, '\n'))
            quit();

        /* Reset start of line indicator ready for the next line */
        sol = 1;
        /* Clear previous match len as this does not carry from one line to the next */
        prev_len = 0;
        /* Move to the next line if doing newline sensitive matching */
        if (!nl_insen && q != NULL)
            line = q + 1;
    } while (!nl_insen && q != NULL);

  clean_up:
    free(cr);
    free(t);

    /* Terminate buf in case it is used as a string */
    if (!ret && put_ch(result, '\0'))
        ret = 1;
    /* Free buffer on failure */
    if (ret) {
        free_buf(result);
        return NULL;
    }

    if (result != NULL)
        res_str = result->a;
    free_buf_wrapping(result);

    return res_str;
}

char *regex_search(char *str, char *find, int nl_insen, int *err)
{
    /*
     * Regular expression search. Returns a pointer to the first match
     * of find in str, or NULL upon no match or error (and sets err
     * to 1 upon error).
     */
    char *t = NULL, *text, *line, *q = NULL;
    char *p;                    /* Pointer to regex match */
    size_t len = 0;             /* Length of regex match */
    struct atom *cr = NULL;
    struct cap_grp cg[NUM_CAP_GRP];
    struct hook hk;

    /* Compile regex expression */
    if ((cr = compile_regex(find, cg, &hk)) == NULL) {
        *err = 1;
        return NULL;
    }

    /*
     * Copy string if in newline sensitive mode as the \n chars will be
     * replaced with \0.
     */
    if (!nl_insen) {
        if ((t = strdup(str)) == NULL) {
            free(cr);
            *err = 1;
            return NULL;
        }
        text = t;
    } else {
        text = str;
    }

    /*
     * Do not process an empty string (but an in-the-middle line can be empty).
     * Not an error.
     */
    if (!*text)
        goto no_match;

    line = text;

    /* Process line by line */
    do {
        /* Terminate line */
        if (!nl_insen) {
            if ((q = strchr(line, '\n')) != NULL)
                *q = '\0';
            /* Do not process the last line if empty. Not an error. */
            else if (!*line)
                goto no_match;
        }

        /* See if there is any match on a line */
        if ((p = match_regex(cr, &hk, line, 1, &len)) != NULL) {
            free(cr);
            free(t);
            /* Make the location relative to the original string */
            return str + (p - text);
        }

        /* Move to the next line if doing newline sensitive matching */
        if (!nl_insen && q != NULL)
            line = q + 1;
    } while (!nl_insen && q != NULL);

  no_match:
    free(cr);
    free(t);
    return NULL;
}
