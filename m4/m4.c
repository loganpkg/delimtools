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

/* m4 */

/* Assumes NULL pointers are zero */

#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define INIT_BUF_SIZE 2

#define HASH_TABLE_SIZE 100

/* size_t Addition OverFlow test */
#define AOF(a, b) ((a) > SIZE_MAX - (b))
/* size_t Multiplication OverFlow test */
#define MOF(a, b) ((a) && (b) > SIZE_MAX / (a))

#define getch(b, read_stdin) (b->i ? *(b->a + --b->i) : (read_stdin ? getchar() : EOF))

#define GAP_SIZE(b) (b->s - b->i)

struct buf {
    char *a;
    size_t i;
    size_t s;
};

/*
 * For hash table entries. Multpile entries at the same hash value link
 * together to form a singularly linked list.
 */
struct entry {
    struct entry *next;
    char *name;
    char *def;
};

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

struct buf *init_buf(void)
{
    struct buf *b;
    if ((b = malloc(sizeof(struct buf))) == NULL)
        return NULL;
    if ((b->a = malloc(INIT_BUF_SIZE)) == NULL) {
        free(b);
        return NULL;
    }
    b->s = INIT_BUF_SIZE;
    b->i = 0;
    return b;
}

void free_buf(struct buf *b)
{
    if (b != NULL) {
        free(b->a);
        free(b);
    }
}

int grow_buf(struct buf *b, size_t will_use)
{
    char *t;
    size_t new_s;
    /* Gap is big enough, nothing to do */
    if (will_use <= GAP_SIZE(b))
        return 0;
    if (MOF(b->s, 2))
        return 1;
    new_s = b->s * 2;
    if (AOF(new_s, will_use))
        new_s += will_use;
    if ((t = realloc(b->a, new_s)) == NULL)
        return 1;
    b->a = t;
    b->s = new_s;
    return 0;
}

int ungetch(struct buf *b, int ch)
{
    if (b->i == b->s)
        if (grow_buf(b, 1))
            return EOF;
    return *(b->a + b->i++) = ch;
}

int filesize(char *fn, size_t * fs)
{
    /* Gets the filesize of a filename */
    struct stat st;
    if (stat(fn, &st))
        return 1;
    if (!S_ISREG(st.st_mode))
        return 1;
    if (st.st_size < 0)
        return 1;
    *fs = st.st_size;
    return 0;
}

int include(struct buf *b, char *fn)
{
    FILE *fp;
    int x;
    size_t fs;
    size_t back_i;

    if (filesize(fn, &fs))
        return 1;
    if (fs > GAP_SIZE(b) && grow_buf(b, fs))
        return 1;
    if ((fp = fopen(fn, "r")) == NULL)
        return 1;
    back_i = b->i + fs;

    errno = 0;
    while ((x = getc(fp)) != EOF)
        *(b->a + --back_i) = x;

    if (errno) {
        fclose(fp);
        return 1;
    }
    if (fclose(fp))
        return 1;

    /* Check */
    if (back_i != b->i)
        return 1;

    /* Success */
    b->i += fs;

    return 0;
}

void delete_buf(struct buf *b)
{
    b->i = 0;
}

int getword(struct buf *token, struct buf *input, int read_stdin, int *err)
{
    int x;
    delete_buf(token);

    /* Always read at least one char */
    errno = 0;
    if ((x = getch(input, read_stdin)) == EOF) {
        if (errno)
            *err = 1;
        return 1;
    }
    /* EOF, no error */
    if (ungetch(token, x) == EOF)
        return 1;

    if (isalpha(x) || x == '_') {
        /* Could be the start of a macro name */
        while (1) {
            /* Read another char */
            errno = 0;
            if ((x = getch(input, read_stdin)) == EOF) {
                if (errno)
                    *err = 1;
                return 1;
            }
            if (!(isalnum(x) || x == '_')) {
                /* Read past the end of the token, so put the char back */
                if (ungetch(input, x) == EOF) {
                    *err = 1;
                    return 1;
                }
                break;
            } else {
                /* Store the char */
                if (ungetch(token, x) == EOF) {
                    *err = 1;
                    return 1;
                }
            }
        }
    }
    /* Null terminate token */
    if (ungetch(token, '\0') == EOF) {
        *err = 1;
        return 1;
    }
    return 0;
}

int ungetstr(struct buf *b, char *s)
{
    size_t len = strlen(s);
    while (len)
        if (ungetch(b, *(s + --len)) == EOF)
            return 1;
    return 0;
}

struct entry *init_entry(void)
{
    struct entry *e;
    if ((e = malloc(sizeof(struct entry))) == NULL)
        return NULL;
    e->next = NULL;
    e->name = NULL;
    e->def = NULL;
    return e;
}

size_t hash_str(char *s)
{
    /* djb2 */
    unsigned char c;
    size_t h = 5381;
    while ((c = *s++))
        h = h * 33 ^ c;
    return h % HASH_TABLE_SIZE;
}

struct entry *lookup_entry(struct entry **ht, char *name)
{
    size_t h = hash_str(name);
    struct entry *e = *(ht + h);
    while (e != NULL) {
        /* Found */
        if (!strcmp(name, e->name))
            return e;
        e = e->next;
    }
    /* Not found */
    return NULL;
}

char *get_def(struct entry **ht, char *name)
{
    struct entry *e;
    if ((e = lookup_entry(ht, name)) == NULL)
        return NULL;
    return e->def;
}

int upsert_entry(struct entry **ht, char *name, char *def)
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
        h = hash_str(name);
        /* Link new entry in at head of list (the existing head could be NULL) */
        e->next = *(ht + h);
        *(ht + h) = e;
    } else {
        /* Update entry: */
        if (def != NULL && (t = strdup(def)) == NULL)
            return 1;
        free(e->def);
        e->def = t;             /* Could be NULL */
    }
    return 0;
}

int delete_entry(struct entry **ht, char *name)
{
    size_t h = hash_str(name);
    struct entry *e = *(ht + h), *prev = NULL;
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
            *(ht + h) = NULL;
        return 0;
    }

    /* Not found */
    return 1;
}

void free_hash_table(struct entry **ht)
{
    struct entry *e, *ne;
    size_t j;
    if (ht != NULL) {
        for (j = 0; j < HASH_TABLE_SIZE; ++j) {
            e = *(ht + j);
            while (e != NULL) {
                ne = e->next;
                free(e->name);
                free(e->def);
                free(e);
                e = ne;
            }
        }
        free(ht);
    }
}

void free_mcall(struct mcall *m)
{
    size_t j;
    if (m != NULL) {
        free(m->name);
        free(m->def);
        for (j = 1; j < 10; ++j)
            free_buf(*(m->arg_buf + j));
        free(m);
    }
}

struct mcall *init_mcall(void)
{
    struct mcall *m;
    size_t j;
    if ((m = calloc(1, sizeof(struct mcall))) == NULL)
        return NULL;
    /* Arg 0 is not used */
    for (j = 1; j < 10; ++j)
        if ((*(m->arg_buf + j) = init_buf()) == NULL) {
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

int put_str(struct buf *b, char *s)
{
    size_t len = strlen(s);
    if (len > GAP_SIZE(b) && grow_buf(b, len))
        return 1;
    memcpy(b->a + b->i, s, len);
    b->i += len;
    return 0;
}

int sub_args(struct buf *result, struct mcall *stack)
{
    int dollar_enc = 0;
    char *s = stack->def, ch;
    struct buf *b;
    delete_buf(result);
    while ((ch = *s++)) {
        if (ch == '$') {
            dollar_enc = 1;
        } else if (dollar_enc && isdigit(ch) && ch != '0') {
            b = *(stack->arg_buf + (ch - '0'));
            if (b->i > GAP_SIZE(result) && grow_buf(result, b->i))
                return 1;
            memcpy(result->a + result->i, b->a, b->i);
            result->i += b->i;
            dollar_enc = 0;
        } else {
            if (dollar_enc && ungetch(result, '$') == EOF)
                return 1;
            if (ungetch(result, ch) == EOF)
                return 1;
        }
    }
    if (ungetch(result, '\0') == EOF)
        return 1;
    return 0;
}

char *strip_def(char *def)
{
    char *d, *t, ch;
    int dollar_enc = 0;
    if ((d = strdup(def)) == NULL)
        return NULL;
    t = d;
    while ((ch = *def++)) {
        if (ch == '$') {
            dollar_enc = 1;
        } else if (dollar_enc && isdigit(ch) && ch != '0') {
            dollar_enc = 0;
        } else {
            if (dollar_enc)
                *t++ = '$';
            *t++ = ch;
        }
    }
    *t = '\0';
    return d;
}

int terminate_args(struct mcall *stack)
{
    size_t j;
    for (j = 1; j < 10; j++)
        if (ungetch(*(stack->arg_buf + j), '\0'))
            return 1;
    return 0;
}

#define QUIT do { \
    ret = 1; \
    goto clean_up; \
} while (0)

int main(int argc, char **argv)
{
    int ret = 0, read_stdin = 1, err, j;
    struct buf *input = NULL, *token = NULL, *next_token = NULL, *result =
        NULL;
    struct entry **ht = NULL, *e;
    int quote_on = 0;
    size_t quote_depth = 0, fs, total_fs = 0, act_div = 0, k;
    /* Diversion 10 is -1 */
    struct buf *diversion[11] = { NULL };
    struct buf *output;
    char left_quote[2] = { '[', '\0' };
    char right_quote[2] = { ']', '\0' };
    struct mcall *stack = NULL;
    char *sd = NULL, *tmp_str = NULL, *p;

    if (argc < 1)
        return 1;

    /* Setup buffers */
    if ((input = init_buf()) == NULL)
        QUIT;
    if ((token = init_buf()) == NULL)
        QUIT;
    if ((next_token = init_buf()) == NULL)
        QUIT;
    if ((result = init_buf()) == NULL)
        QUIT;

    /* Setup diversions */
    for (k = 0; k < 11; ++k)
        if ((*(diversion + k) = init_buf()) == NULL)
            QUIT;
    output = *diversion;

    if ((ht = calloc(HASH_TABLE_SIZE, sizeof(struct entry *))) == NULL)
        QUIT;

    /* Define built-in macros. They have a def of NULL. */
    if (upsert_entry(ht, "define", NULL))
        QUIT;
    if (upsert_entry(ht, "undefine", NULL))
        QUIT;
    if (upsert_entry(ht, "changequote", NULL))
        QUIT;
    if (upsert_entry(ht, "divert", NULL))
        QUIT;
    if (upsert_entry(ht, "dumpdef", NULL))
        QUIT;
    if (upsert_entry(ht, "errprint", NULL))
        QUIT;
    if (upsert_entry(ht, "ifdef", NULL))
        QUIT;
    if (upsert_entry(ht, "ifelse", NULL))
        QUIT;
    if (upsert_entry(ht, "include", NULL))
        QUIT;
    if (upsert_entry(ht, "len", NULL))
        QUIT;
    if (upsert_entry(ht, "index", NULL))
        QUIT;

    if (argc > 1) {
        /* Do not read stdin if there are command line files */
        read_stdin = 0;
        /* Get total size of command line files */
        for (j = argc - 1; j; --j) {
            if (filesize(*(argv + j), &fs))
                QUIT;
            total_fs += fs;
        }
        /* Make sure buffer is big enough to hold all files */
        if (grow_buf(input, total_fs))
            QUIT;
        /* Load command line files into buffer */
        for (j = argc - 1; j; --j)
            if (include(input, *(argv + j)))
                QUIT;
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
#define ISMACRO(s) ((isalpha(*s) || *s == '_') \
    && (e = lookup_entry(ht, s)) != NULL)

/* Set output to stack argument collection buffer or diversion buffer */
#define SET_OUTPUT output = (stack == NULL ? *(diversion + act_div) \
    : *(stack->arg_buf + stack->act_arg))

#define DIV(n) (*(diversion + n))

#define OUT_DIV(n) do { \
    if (DIV(n)->i && fwrite(DIV(n)->a, 1, DIV(n)->i, stdout) != DIV(n)->i) \
        QUIT; \
    DIV(n)->i = 0; \
} while (0)

/* Tests if a string consists of a single whitespace character */
#define WS(s) (!strcmp(s, " ") || !strcmp(s, "\t") || !strcmp(s, "\n") \
    || !strcmp(s, "\r"))

/* Remove stack head and set ouput */
#define REMOVE_SH do { \
    delete_stack_head(&stack); \
    SET_OUTPUT; \
} while (0)

/* Stack macro collected argument number n */
#define ARG(n) (*(stack->arg_buf + n))->a

#define READ_TOKEN(t) do { \
    err = 0; \
    if (getword(t, input, read_stdin, &err)) { \
        if (err) \
            QUIT; \
        else \
            goto end_of_input; \
    } \
} while(0)

/* Eat whitespace */
#define EAT_WS do { \
    do \
        READ_TOKEN(next_token); \
    while (WS(NTS)); \
    if (ungetstr(input, NTS)) QUIT; \
} while (0)

#define SN stack->name

#define EMSG(m) fprintf(stderr, m "\n")

#define EQUIT(m) do { \
    EMSG(m); \
    QUIT; \
} while (0)

/* Process built-in macro with args */
#define PROCESS_BI_WITH_ARGS do { \
    if (!strcmp(SN, "define")) { \
        if (upsert_entry(ht, ARG(1), ARG(2))) \
            QUIT; \
    } else if (!strcmp(SN, "undefine")) { \
        if (delete_entry(ht, ARG(1))) \
            QUIT; \
    } else if (!strcmp(SN, "changequote")) { \
        if (strlen(ARG(1)) != 1 || strlen(ARG(2)) != 1 || *ARG(1) == *ARG(2) \
            || !isgraph(*ARG(1)) || !isgraph(*ARG(2)) \
            || *ARG(1) == '(' || *ARG(2) == '(' \
            || *ARG(1) == ')' || *ARG(2) == ')' \
            || *ARG(1) == ',' || *ARG(2) == ',') { \
            EQUIT("changequote: quotes must be different single graph chars" \
                " that cannot a comma or parentheses"); \
        } \
        *left_quote = *ARG(1); \
        *right_quote = *ARG(2); \
    } else if (!strcmp(SN, "divert")) { \
        if (strlen(ARG(1)) == 1 && isdigit(*ARG(1))) \
            act_div = *ARG(1) - '0'; \
        else if (!strcmp(ARG(1), "-1")) \
            act_div = 10; \
        else \
            EQUIT("divert: Diversion number must be 0 to 9 or -1"); \
        SET_OUTPUT; \
    } else if (!strcmp(SN, "dumpdef")) { \
        for (k = 1; k < 10; ++k) { \
            if (ISMACRO(ARG(k))) \
                fprintf(stderr, "%s: %s\n", ARG(k), \
                    e->def == NULL ? "built-in" : e->def); \
            else if (*ARG(k) != '\0') \
                fprintf(stderr, "%s: undefined\n", ARG(k)); \
        } \
    } else if (!strcmp(SN, "errprint")) { \
        for (k = 1; k < 10; ++k) \
            if (*ARG(k) != '\0') \
                fprintf(stderr, "%s\n", ARG(k)); \
    } else if (!strcmp(SN, "ifdef")) { \
        if (ungetstr(input, ISMACRO(ARG(1)) ? ARG(2) : ARG(3))) \
            QUIT; \
    } else if (!strcmp(SN, "ifelse")) { \
        if (ungetstr(input, !strcmp(ARG(1), ARG(2)) ? ARG(3) : ARG(4))) \
            QUIT; \
    } else if (!strcmp(SN, "include")) { \
        if (include(input, ARG(1))) { \
            fprintf(stderr, "include: Failed to include file: %s\n", ARG(1)); \
            QUIT; \
        } \
    } else if (!strcmp(SN, "len")) { \
        if (asprintf(&tmp_str, "%lu", strlen(ARG(1))) == -1) \
            QUIT; \
        if (ungetstr(input, tmp_str)) \
            QUIT; \
        free(tmp_str); \
        tmp_str = NULL; \
    } else if (!strcmp(SN, "index")) { \
        p = strstr(ARG(1), ARG(2)); \
        if (p == NULL) { \
            if (asprintf(&tmp_str, "%d", -1) == -1) \
                QUIT; \
        } else { \
            if (asprintf(&tmp_str, "%lu", p - ARG(1)) == -1) \
                QUIT; \
        } \
        if (ungetstr(input, tmp_str)) \
            QUIT; \
        free(tmp_str); \
        tmp_str = NULL; \
    } \
} while (0)


    /* m4 loop: read input word by word */
    while (1) {
        /* Write diversion 0 (for interactive use) */
        OUT_DIV(0);
        /* Read token */
        READ_TOKEN(token);

        if (!strcmp(TS, left_quote)) {
            if (!quote_on)
                quote_on = 1;
            if (quote_depth && put_str(output, TS))
                QUIT;
            ++quote_depth;
        } else if (!strcmp(TS, right_quote)) {
            if (quote_depth > 1 && put_str(output, TS))
                QUIT;
            if (!--quote_depth)
                quote_on = 0;
        } else if (quote_on) {
            if (put_str(output, TS))
                QUIT;
        } else if (ISMACRO(TS)) {
            /* Token match */
            READ_TOKEN(next_token);

            if (!strcmp(NTS, "(")) {
                /* Start of macro with arguments */
                /* Add macro call to stack */
                if (stack_on_mcall(&stack))
                    QUIT;
                /* Copy macro name */
                if ((stack->name = strdup(TS)) == NULL)
                    QUIT;
                /* Copy macro definition (built-ins will be NULL) */
                if (e->def != NULL
                    && (stack->def = strdup(e->def)) == NULL)
                    QUIT;
                /* Increment bracket depth for this first bracket */
                ++stack->bracket_depth;
                SET_OUTPUT;
                EAT_WS;
            } else {
                /* Macro call without arguments */
                /* Put the next token back into the input */
                if (ungetstr(input, NTS))
                    QUIT;
                /* User defined macro */
                /* Strip dollar argument positions from definition */
                if ((sd = strip_def(e->def)) == NULL)
                    QUIT;
                if (ungetstr(input, sd))
                    QUIT;
                free(sd);
                sd = NULL;
            }
        } else if (ARG_END) {
            /* End of argument collection */
            /* Decrement bracket depth for bracket just encountered */
            --stack->bracket_depth;
            if (stack->def == NULL) {
                /* Define built-in macro */
                if (terminate_args(stack))
                    QUIT;
                PROCESS_BI_WITH_ARGS;
            } else if (stack->def != NULL) {
                /* User defined macro */
                if (sub_args(result, stack))
                    QUIT;
                if (ungetstr(input, result->a))
                    QUIT;
            }
            REMOVE_SH;
        } else if (ARG_COMMA) {
            if (stack->act_arg == 9)
                EQUIT("Macro call has too many arguments");
            ++stack->act_arg;
            SET_OUTPUT;
            EAT_WS;
        } else if (NESTED_CB) {
            if (put_str(output, TS))
                QUIT;
            --stack->bracket_depth;
        } else if (NESTED_OB) {
            if (put_str(output, TS))
                QUIT;
            ++stack->bracket_depth;
        } else {
            /* Pass through token */
            if (put_str(output, TS))
                QUIT;
        }
    }

  end_of_input:

    /* Checks */
    if (stack != NULL)
        EQUIT("Input finished without unwinding the stack");
    if (quote_on)
        EQUIT("Input finished without exiting quotes");

    for (k = 0; k < 10; k++)
        OUT_DIV(k);

  clean_up:
    free_buf(input);
    free_buf(token);
    free_buf(next_token);
    free_buf(result);
    for (k = 0; k < 11; ++k)
        free_buf(*(diversion + k));
    free_hash_table(ht);
    free_stack(stack);
    free(sd);
    free(tmp_str);
    return ret;
}
