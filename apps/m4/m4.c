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

/*  An implementation of the m4 macro processor */

/*
 * References:
 * Brian W. Kernighan and Dennis M. Ritchie, The M4 Macro Processor,
 *     Bell Laboratories, Murray Hill, New Jersey 07974, July 1, 1977.
 */

/* Set to 1 to enable the esyscmd and maketemp built-in macros */
#define ESYSCMD_MAKETEMP 1

#ifdef __linux__
/* For strdup and popen */
#define _XOPEN_SOURCE 500
#endif

#if ESYSCMD_MAKETEMP && !defined _WIN32
#include <sys/wait.h>
#endif

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#include "../../mods/gen/gen.h"
#include "../../mods/buf/buf.h"
#include "../../mods/hashtable/hashtable.h"
#include "../../mods/fs/fs.h"


#if ESYSCMD_MAKETEMP && defined _WIN32
#define popen _popen
#define pclose _pclose
#endif

#define INIT_BUF_SIZE 512

#define HASH_TABLE_SIZE 16384


/*
 * mcall used to stack on nested macro calls.
 * Only argument buffers 1 to 9 are used (index 0 will be NULL).
 */
struct mcall {
    struct mcall *next;
    char *name;                 /* Macro name */
    char *def;                  /* Macro definition before substitution */
    size_t bracket_depth;       /* Only unquoted brackets are counted */
    size_t act_arg;             /* The current argument being collected */
    struct buf *arg_buf[10];    /* For argument collection */
};

#if ESYSCMD_MAKETEMP
int esyscmd(struct buf *input, struct buf *tmp_buf, char *cmd)
{
    FILE *fp;
    int x, status;
    delete_buf(tmp_buf);

#ifdef _WIN32
#define R_STR "rb"
#else
#define R_STR "r"
#endif
    if ((fp = popen(cmd, R_STR)) == NULL)
        return 1;

    errno = 0;
    while ((x = getc(fp)) != EOF)
        if (x != '\0' && unget_ch(tmp_buf, x)) {
            pclose(fp);
            return 1;
        }
    if (errno) {
        pclose(fp);
        return 1;
    }
    if ((status = pclose(fp)) == -1)
        return 1;
#ifdef _WIN32
    if (status)
        return 1;
#else
#define EXIT_OK (WIFEXITED(status) && !WEXITSTATUS(status))
    if (!EXIT_OK)
        return 1;
#endif
    if (unget_ch(tmp_buf, '\0'))
        return 1;
    if (unget_str(input, tmp_buf->a))
        return 1;
    return 0;
}
#endif


void free_mcall(struct mcall *m)
{
    size_t j = 1;
    if (m != NULL) {
        free(m->name);
        free(m->def);
        while (*(m->arg_buf + j) != NULL && j < 10) {
            free_buf(*(m->arg_buf + j));
            ++j;
        }
        free(m);
    }
}

struct mcall *init_mcall(void)
{
    struct mcall *m;
    if ((m = calloc(1, sizeof(struct mcall))) == NULL)
        return NULL;
    /*
     * Arg 0 is not used. Only allocate the buffer for arg 1.
     * The others will be allocated on demand.
     */
    if ((*(m->arg_buf + 1) = init_buf(INIT_BUF_SIZE)) == NULL) {
        free_mcall(m);
        return NULL;
    }
    m->act_arg = 1;
    return m;
}

int stack_on_mcall(struct mcall **stack)
{
    struct mcall *t;
    if ((t = init_mcall()) == NULL)
        return 1;

    /* Link new head on */
    if (*stack != NULL)
        t->next = *stack;

    *stack = t;
    return 0;
}

void delete_stack_head(struct mcall **stack)
{
    struct mcall *m;
    if (*stack == NULL)
        return;
    m = (*stack)->next;
    free_mcall(*stack);
    *stack = m;
}

void free_stack(struct mcall *stack)
{
    struct mcall *m = stack, *next;
    while (m != NULL) {
        next = m->next;
        free_mcall(m);
        m = next;
    }
}

int sub_args(struct buf *result, struct mcall *stack)
{
    char *s = stack->def, ch, h;
    struct buf *b;
    delete_buf(result);
    while ((ch = *s++)) {
        if (ch == '$') {
            /* Look ahead */
            h = *s;
            if (isdigit(h) && h != '0') {
                /* Arg */
                if ((b = *(stack->arg_buf + (h - '0'))) != NULL
                    && put_mem(result, b->a, b->i))
                    return 1;
                ++s;            /* Eat */
            } else {
                if (unget_ch(result, ch))
                    return 1;
            }
        } else {
            if (unget_ch(result, ch))
                return 1;
        }
    }
    if (unget_ch(result, '\0'))
        return 1;

    return 0;
}

char *strip_def(char *def)
{
    char *d, *t, ch, h;
    if ((d = strdup(def)) == NULL)
        return NULL;
    t = d;
    while ((ch = *def++)) {
        if (ch == '$') {
            /* Look ahead */
            h = *def;
            if (isdigit(h) && h != '0')
                ++def;          /* Eat */
            else
                *t++ = ch;
        } else {
            *t++ = ch;
        }
    }
    *t = '\0';
    return d;
}

int terminate_args(struct mcall *stack)
{
    size_t j = 1;
    while (*(stack->arg_buf + j) != NULL && j < 10) {
        if (unget_ch(*(stack->arg_buf + j), '\0'))
            return 1;
        ++j;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int ret = 0, read_stdin = 1, r, j;
#if ESYSCMD_MAKETEMP && !defined _WIN32
    int fd;
#endif
    struct buf *input = NULL, *token = NULL, *next_token = NULL, *result =
        NULL, *tmp_buf = NULL;
    struct hashtable *ht = NULL;
    struct entry *e;
    int quote_on = 0;
    size_t quote_depth = 0, fs, total_fs = 0, act_div = 0, k, len, w, n;
    /* Diversion 10 is -1 */
    struct buf *diversion[11] = { NULL };
    struct buf *output;
    char left_quote[2] = { '`', '\0' };
    char right_quote[2] = { '\'', '\0' };
    struct mcall *stack = NULL;
#define NUM_SIZE 24
    char *sd = NULL, *tmp_str = NULL, num[NUM_SIZE], *p, *q;
    unsigned char uc, uc2;
    int map[UCHAR_NUM], x;

    if (argc < 1)
        return 1;

#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1)
        return 1;
    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        return 1;
    if (_setmode(_fileno(stderr), _O_BINARY) == -1)
        return 1;
#endif

    /* Setup buffers */
    if ((token = init_buf(INIT_BUF_SIZE)) == NULL)
        quit();
    if ((next_token = init_buf(INIT_BUF_SIZE)) == NULL)
        quit();
    if ((result = init_buf(INIT_BUF_SIZE)) == NULL)
        quit();
    if ((tmp_buf = init_buf(INIT_BUF_SIZE)) == NULL)
        quit();

    /* Setup diversions */
    for (k = 0; k < 11; ++k)
        if ((*(diversion + k) = init_buf(INIT_BUF_SIZE)) == NULL)
            quit();
    output = *diversion;

    if ((ht = init_hashtable(HASH_TABLE_SIZE)) == NULL)
        quit();

    /* Define built-in macros. They have a def of NULL. */
    if (upsert_entry(ht, "define", NULL))
        quit();
    if (upsert_entry(ht, "undefine", NULL))
        quit();
    if (upsert_entry(ht, "changequote", NULL))
        quit();
    if (upsert_entry(ht, "divert", NULL))
        quit();
    if (upsert_entry(ht, "dumpdef", NULL))
        quit();
    if (upsert_entry(ht, "errprint", NULL))
        quit();
    if (upsert_entry(ht, "ifdef", NULL))
        quit();
    if (upsert_entry(ht, "ifelse", NULL))
        quit();
    if (upsert_entry(ht, "include", NULL))
        quit();
    if (upsert_entry(ht, "len", NULL))
        quit();
    if (upsert_entry(ht, "index", NULL))
        quit();
    if (upsert_entry(ht, "translit", NULL))
        quit();
    if (upsert_entry(ht, "substr", NULL))
        quit();
    if (upsert_entry(ht, "dnl", NULL))
        quit();
    if (upsert_entry(ht, "divnum", NULL))
        quit();
    if (upsert_entry(ht, "undivert", NULL))
        quit();
#if ESYSCMD_MAKETEMP
    if (upsert_entry(ht, "esyscmd", NULL))
        quit();
    if (upsert_entry(ht, "maketemp", NULL))
        quit();
#endif
    if (upsert_entry(ht, "incr", NULL))
        quit();
    if (upsert_entry(ht, "htdist", NULL))
        quit();
    if (upsert_entry(ht, "dirsep", NULL))
        quit();
    if (upsert_entry(ht, "add", NULL))
        quit();
    if (upsert_entry(ht, "mult", NULL))
        quit();
    if (upsert_entry(ht, "sub", NULL))
        quit();
    if (upsert_entry(ht, "div", NULL))
        quit();
    if (upsert_entry(ht, "mod", NULL))
        quit();

    if (argc > 1) {
        /* Do not read stdin if there are command line files */
        read_stdin = 0;
        /* Get total size of command line files */
        for (j = argc - 1; j; --j) {
            if (filesize(*(argv + j), &fs))
                quit();
            total_fs += fs;
        }
        /* Initialise input buffer big enough to hold all files */
        if ((input = init_buf(total_fs)) == NULL)
            quit();
        /* Load command line files into buffer */
        for (j = argc - 1; j; --j)
            if (include(input, *(argv + j)))
                quit();
    } else {
        if ((input = init_buf(INIT_BUF_SIZE)) == NULL)
            quit();
    }

/* Token string */
#define TS token->a

/* Next token string */
#define NTS next_token->a

/* End of argument collection */
#define ARG_END (stack != NULL && stack->bracket_depth == 1 \
    && !strcmp(TS, ")"))

/* Nested close bracket (unquoted) */
#define NESTED_CB (stack != NULL && stack->bracket_depth > 1 \
    && !strcmp(TS, ")"))

/* Nested open bracket (unquoted) */
#define NESTED_OB (stack != NULL && !strcmp(TS, "("))

/* Argument comma in a macro call */
#define ARG_COMMA (stack != NULL && stack->bracket_depth == 1 \
    && !strcmp(TS, ","))

/* String s is a match of a macro name */
#define ismacro(s) ((isalpha(*s) || *s == '_') \
    && (e = lookup_entry(ht, s)) != NULL)

/* Set output to stack argument collection buffer or diversion buffer */
#define set_output() output = (stack == NULL ? *(diversion + act_div) \
    : *(stack->arg_buf + stack->act_arg))

#define div(n) (*(diversion + n))

#define out_div(n) do { \
    if (div(n)->i && fwrite(div(n)->a, 1, div(n)->i, stdout) != div(n)->i) \
        quit(); \
    div(n)->i = 0; \
} while (0)

#define undivert_all() for (k = 0; k < 10; k++) \
    out_div(k)

/* Tests if a string consists of a single whitespace character */
#define ws(s) (!strcmp(s, " ") || !strcmp(s, "\t") || !strcmp(s, "\n") \
    || !strcmp(s, "\r"))

/* Remove stack head and set ouput */
#define remove_sh() do { \
    delete_stack_head(&stack); \
    set_output(); \
} while (0)

/* Stack macro collected argument number n */
#define arg(n) (*(stack->arg_buf + n) == NULL ? "" : (*(stack->arg_buf + n))->a)

#define read_token(t) do { \
    r = 0; \
    if ((r = get_word(t, input, read_stdin))) { \
        if (r == 1) \
            quit(); \
        else \
            goto end_of_input; \
    } \
} while(0)

/* Eat whitespace */
#define eat_ws() do { \
    do \
        read_token(next_token); \
    while (ws(NTS)); \
    if (unget_str(input, NTS)) \
        quit(); \
} while (0)

/* Delete to new line (inclusive) */
#define dnl() do { \
    do \
        read_token(next_token); \
    while (strcmp(NTS, "\n")); \
} while (0)

#define unget_divnum() do { \
    if (act_div == 10) \
        snprintf(num, NUM_SIZE, "%d", -1); \
    else \
        snprintf(num, NUM_SIZE, "%lu", (unsigned long) act_div); \
    if (unget_str(input, num)) \
        quit(); \
} while (0)

#define SN stack->name

#define emsg(m) fprintf(stderr, m "\n")

#define equit(m) do { \
    emsg(m); \
    quit(); \
} while (0)

#if ESYSCMD_MAKETEMP
#ifdef _WIN32
/* No integer overflow risk, as already a string. */
/* This function does not actually create the file */
#define maketemp(s) if (_mktemp_s(s, strlen(s) + 1)) \
    equit("maketemp: Failed")
#else
#define maketemp(s) do { \
    if ((fd = mkstemp(s)) == -1) \
        equit("maketemp: Failed"); \
    if (close(fd)) \
        equit("maketemp: Failed to close temp file"); \
} while (0)
#endif
#endif

#ifdef _WIN32
#define DIRSEP "\\"
#else
#define DIRSEP "/"
#endif

/* Process built-in macro with args */
#define process_bi_with_args() \
    if (!strcmp(SN, "define")) { \
        if (upsert_entry(ht, arg(1), arg(2))) \
            quit(); \
    } else if (!strcmp(SN, "undefine")) { \
        if (delete_entry(ht, arg(1))) \
            quit(); \
    } else if (!strcmp(SN, "changequote")) { \
        if (strlen(arg(1)) != 1 || strlen(arg(2)) != 1 || *arg(1) == *arg(2) \
            || !isgraph(*arg(1)) || !isgraph(*arg(2)) \
            || *arg(1) == '(' || *arg(2) == '(' \
            || *arg(1) == ')' || *arg(2) == ')' \
            || *arg(1) == ',' || *arg(2) == ',') { \
            equit("changequote: quotes must be different single graph chars" \
                " that cannot a comma or parentheses"); \
        } \
        *left_quote = *arg(1); \
        *right_quote = *arg(2); \
    } else if (!strcmp(SN, "divert")) { \
        if (strlen(arg(1)) == 1 && isdigit(*arg(1))) \
            act_div = *arg(1) - '0'; \
        else if (!strcmp(arg(1), "-1")) \
            act_div = 10; \
        else \
            equit("divert: Diversion number must be 0 to 9 or -1"); \
        set_output(); \
    } else if (!strcmp(SN, "dumpdef")) { \
        for (k = 1; k < 10; ++k) { \
            if (ismacro(arg(k))) \
                fprintf(stderr, "%s: %s\n", arg(k), \
                    e->def == NULL ? "built-in" : e->def); \
            else if (*arg(k) != '\0') \
                fprintf(stderr, "%s: undefined\n", arg(k)); \
        } \
    } else if (!strcmp(SN, "errprint")) { \
        for (k = 1; k < 10; ++k) \
            if (*arg(k) != '\0') \
                fprintf(stderr, "%s\n", arg(k)); \
    } else if (!strcmp(SN, "ifdef")) { \
        if (unget_str(input, ismacro(arg(1)) ? arg(2) : arg(3))) \
            quit(); \
    } else if (!strcmp(SN, "ifelse")) { \
        if (unget_str(input, !strcmp(arg(1), arg(2)) ? arg(3) : arg(4))) \
            quit(); \
    } else if (!strcmp(SN, "include")) { \
        if (include(input, arg(1))) { \
            fprintf(stderr, "include: Failed to include file: %s\n", arg(1)); \
            quit(); \
        } \
    } else if (!strcmp(SN, "len")) { \
        snprintf(num, NUM_SIZE, "%lu", (unsigned long) strlen(arg(1))); \
        if (unget_str(input, num)) \
            quit(); \
    } else if (!strcmp(SN, "index")) { \
        p = strstr(arg(1), arg(2)); \
        if (p == NULL) \
            snprintf(num, NUM_SIZE, "%d", -1); \
        else \
            snprintf(num, NUM_SIZE, "%lu", (unsigned long) (p - arg(1))); \
        if (unget_str(input, num)) \
            quit(); \
    } else if (!strcmp(SN, "translit")) { \
        /* Set mapping to pass through (-1) */ \
        for (k = 0; k < UCHAR_NUM; k++) \
            *(map + k) = -1; \
        p = arg(2); \
        q = arg(3); \
        /* Create mapping while strings are in parallel */ \
        while ((uc = *p++) && (uc2 = *q++)) \
            if (*(map + uc) == -1) /* Preference to first occurrence */ \
                *(map + uc) = uc2; \
        /* Continue first string, setting mapping to delete (\0) */ \
        while (uc != '\0') { \
            *(map + uc) = '\0'; \
            uc = *p++; \
        } \
        if ((tmp_str = strdup(arg(1))) == NULL) \
            quit(); \
        p = arg(1); \
        q = tmp_str; \
        while ((uc = *p++)) { \
            x = *(map + uc); \
            if (x == -1) \
                *q++ = uc; \
            else if (x != '\0') \
                *q++ = x; \
        } \
        *q = '\0'; \
        if (unget_str(input, tmp_str)) \
            quit(); \
        free(tmp_str); \
        tmp_str = NULL; \
    } else if (!strcmp(SN, "substr")) { \
        if ((len = strlen(arg(1)))) { \
            if (str_to_num(arg(2), &w) || str_to_num(arg(3), &n)) \
                equit("substr: Invalid index or length"); \
            if (w < len) { \
                /* Do not need to check for overflow here */ \
                if ((tmp_str = malloc(len + 1)) == NULL) \
                    quit(); \
                if (aof(n, 1)) \
                    quit(); \
                snprintf(tmp_str, min(len + 1, n + 1), "%s", arg(1) + w); \
                if (unget_str(input, tmp_str)) \
                    quit(); \
                free(tmp_str); \
                tmp_str = NULL; \
            } \
        } \
    } else if (!strcmp(SN, "undivert")) { \
        if (!act_div) { \
            /* In diversion 0 */ \
            for (k = 1; k < 10; k++) \
                if (strlen(arg(k)) == 1 && isdigit(*arg(k)) && *arg(k) != '0') \
                    out_div(*arg(k) - '0'); \
        } else { \
            /* Cannot undivert division 0 or the active diversion */ \
            for (k = 1; k < 10; k++) \
                if (strlen(arg(k)) == 1 && isdigit(*arg(k)) && *arg(k) != '0' \
                    && (size_t) (*arg(k) - '0') != act_div \
                    && buf_dump_buf(div(act_div), div((*arg(k) - '0')))) \
                        quit(); \
        } \
    } else if (!strcmp(SN, "dnl")) { \
        dnl(); \
    } else if (!strcmp(SN, "divnum")) { \
        unget_divnum(); \
    } else if (!strcmp(SN, "incr")) { \
        if(str_to_num(arg(1), &n)) \
            equit("incr: Invalid number"); \
        if (aof(n, 1)) \
            equit("incr: Integer overflow"); \
        n += 1; \
        snprintf(num, NUM_SIZE, "%lu", (unsigned long) n); \
        if (unget_str(input, num)) \
            quit(); \
    } else if (!strcmp(SN, "htdist")) { \
        htdist(ht); \
    } else if (!strcmp(SN, "dirsep")) { \
        if (unget_str(input, DIRSEP)) \
            quit(); \
    } else if (!strcmp(SN, "add")) { \
        w = 0; \
        for (k = 1; k < 10; ++k) { \
            if (*arg(k) != '\0') { \
                if (str_to_num(arg(k), &n)) \
                    equit("add: Invalid number"); \
                if (aof(w, n)) \
                    equit("add: Integer overflow"); \
                w += n; \
            } \
        } \
        snprintf(num, NUM_SIZE, "%lu", (unsigned long) w); \
        if (unget_str(input, num)) \
            quit(); \
    } else if (!strcmp(SN, "mult")) { \
        w = 1; \
        for (k = 1; k < 10; ++k) { \
            if (*arg(k) != '\0') { \
                if (str_to_num(arg(k), &n)) \
                    equit("mult: Invalid number"); \
                if (mof(w, n)) \
                    equit("mult: Integer overflow"); \
                w *= n; \
            } \
        } \
        snprintf(num, NUM_SIZE, "%lu", (unsigned long) w); \
        if (unget_str(input, num)) \
            quit(); \
    } else if (!strcmp(SN, "sub")) { \
        if (*arg(1) == '\0') \
            equit("sub: Argument 1 must be used"); \
        if (str_to_num(arg(1), &w)) \
            equit("sub: Invalid number"); \
        for (k = 2; k < 10; ++k) { \
            if (*arg(k) != '\0') { \
                if (str_to_num(arg(k), &n)) \
                    equit("sub: Invalid number"); \
                if (n > w) \
                    equit("sub: Integer underflow"); \
                w -= n; \
            } \
        } \
        snprintf(num, NUM_SIZE, "%lu", (unsigned long) w); \
        if (unget_str(input, num)) \
            quit(); \
    } else if (!strcmp(SN, "div")) { \
        if (*arg(1) == '\0') \
            equit("div: Argument 1 must be used"); \
        if (str_to_num(arg(1), &w)) \
            equit("div: Invalid number"); \
        for (k = 2; k < 10; ++k) { \
            if (*arg(k) != '\0') { \
                if (str_to_num(arg(k), &n)) \
                    equit("div: Invalid number"); \
                if (!n) \
                    equit("div: Divide by zero"); \
                w /= n; \
            } \
        } \
        snprintf(num, NUM_SIZE, "%lu", (unsigned long) w); \
        if (unget_str(input, num)) \
            quit(); \
    } else if (!strcmp(SN, "mod")) { \
        if (*arg(1) == '\0') \
            equit("mod: Argument 1 must be used"); \
        if (str_to_num(arg(1), &w)) \
            equit("mod: Invalid number"); \
        for (k = 2; k < 10; ++k) { \
            if (*arg(k) != '\0') { \
                if (str_to_num(arg(k), &n)) \
                    equit("mod: Invalid number"); \
                if (!n) \
                    equit("mod: Modulo by zero"); \
                w %= n; \
            } \
        } \
        snprintf(num, NUM_SIZE, "%lu", (unsigned long) w); \
        if (unget_str(input, num)) \
            quit(); \
    }

#if ESYSCMD_MAKETEMP
/* These tag onto the end of the list of built-in macros with args */
#define process_bi_with_args_extra() \
    else if (!strcmp(SN, "maketemp")) { \
        /* arg(1) is the template string which is modified in-place */ \
        maketemp(arg(1)); \
        if (unget_str(input, arg(1))) \
            quit(); \
    } else if (!strcmp(SN, "esyscmd")) { \
        if (esyscmd(input, tmp_buf, arg(1))) \
            equit("esyscmd: Failed"); \
    }
#endif


/* Process built-in macro with no arguments */
#define process_bi_no_args() do { \
    if (!strcmp(TS, "dnl")) { \
        dnl(); \
    } else if (!strcmp(TS, "divnum")) { \
        unget_divnum(); \
    } else if (!strcmp(TS, "undivert")) { \
        if (act_div) \
            equit("undivert: Can only call from diversion 0" \
                " when called without arguments"); \
        undivert_all(); \
    } else if (!strcmp(TS, "divert")) { \
        act_div = 0; \
        set_output(); \
    } else if (!strcmp(TS, "htdist")) { \
        htdist(ht); \
    } else if (!strcmp(TS, "dirsep")) { \
        if (unget_str(input, DIRSEP)) \
            quit(); \
    } else { \
        /* The remaining macros must take arguments, so pass through */ \
        if (put_str(output, TS)) \
            quit(); \
    } \
} while (0)


    /* m4 loop: read input word by word */
    while (1) {
        /* Write diversion 0 (for interactive use) */
        out_div(0);
        /* Read token */
        read_token(token);

        if (!strcmp(TS, left_quote)) {
            if (!quote_on)
                quote_on = 1;
            if (quote_depth && put_str(output, TS))
                quit();
            ++quote_depth;
        } else if (!strcmp(TS, right_quote)) {
            if (quote_depth > 1 && put_str(output, TS))
                quit();
            if (!--quote_depth)
                quote_on = 0;
        } else if (quote_on) {
            if (put_str(output, TS))
                quit();
        } else if (ismacro(TS)) {
            /* Token match */
            read_token(next_token);

            if (!strcmp(NTS, "(")) {
                /* Start of macro with arguments */
                /* Add macro call to stack */
                if (stack_on_mcall(&stack))
                    quit();
                /* Copy macro name */
                if ((stack->name = strdup(TS)) == NULL)
                    quit();
                /* Copy macro definition (built-ins will be NULL) */
                if (e->def != NULL
                    && (stack->def = strdup(e->def)) == NULL)
                    quit();
                /* Increment bracket depth for this first bracket */
                ++stack->bracket_depth;
                set_output();
                eat_ws();
            } else {
                /* Macro call without arguments (stack is not used) */
                /* Put the next token back into the input */
                if (unget_str(input, NTS))
                    quit();
                if (e->def == NULL) {
                    /* Built-in macro */
                    process_bi_no_args();
                } else {
                    /* User defined macro */
                    /* Strip dollar argument positions from definition */
                    if ((sd = strip_def(e->def)) == NULL)
                        quit();
                    if (unget_str(input, sd))
                        quit();
                    free(sd);
                    sd = NULL;
                }
            }
        } else if (ARG_END) {
            /* End of argument collection */
            /* Decrement bracket depth for bracket just encountered */
            --stack->bracket_depth;
            if (stack->def == NULL) {
                /* Built-in macro */
                if (terminate_args(stack))
                    quit();
                /* Deliberately no semicolons after these macro calls */
                process_bi_with_args()
#if ESYSCMD_MAKETEMP
                    process_bi_with_args_extra()
#endif
            } else {
                /* User defined macro */
                if (sub_args(result, stack))
                    quit();
                if (unget_str(input, result->a))
                    quit();
            }
            remove_sh();
        } else if (ARG_COMMA) {
            if (stack->act_arg == 9)
                equit("Macro call has too many arguments");
            /* Allocate buffer for argument collection */
            if ((*(stack->arg_buf + ++stack->act_arg) =
                 init_buf(INIT_BUF_SIZE)) == NULL)
                quit();
            set_output();
            eat_ws();
        } else if (NESTED_CB) {
            if (put_str(output, TS))
                quit();
            --stack->bracket_depth;
        } else if (NESTED_OB) {
            if (put_str(output, TS))
                quit();
            ++stack->bracket_depth;
        } else {
            /* Pass through token */
            if (put_str(output, TS))
                quit();
        }
    }

  end_of_input:

    /* Checks */
    if (stack != NULL)
        equit("Input finished without unwinding the stack");
    if (quote_on)
        equit("Input finished without exiting quotes");

    undivert_all();

  clean_up:
    free_buf(input);
    free_buf(token);
    free_buf(next_token);
    free_buf(result);
    free_buf(tmp_buf);
    for (k = 0; k < 11; ++k)
        free_buf(*(diversion + k));
    free_hashtable(ht);
    free_stack(stack);
    free(sd);
    free(tmp_str);
    return ret;
}
