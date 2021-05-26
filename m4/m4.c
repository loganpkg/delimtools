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

#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>

#define INIT_BUF_SIZE 2

/* size_t Addition OverFlow test */
#define AOF(a, b) ((a) > SIZE_MAX - (b))
/* size_t Multiplication OverFlow test */
#define MOF(a, b) ((a) && (b) > SIZE_MAX / (a))

struct buf {
    char *a;
    size_t i;
    size_t s;
};

#define getch(b) (b->i ? *(b->a + --b->i) : getchar())

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
    if (fs > b->s - b->i && grow_buf(b, fs))
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

    /* Success */
    b->i += fs;

    return 0;
}

void delete_buf(struct buf *b)
{
    b->i = 0;
}

int getword(struct buf *token, struct buf *input, int *err)
{
    int x;
    delete_buf(token);

    /* Always read at least one char */
    errno = 0;
    if ((x = getch(input)) == EOF) {
        if (errno)
            *err = 1;
        return 1;
    }
    if (ungetch(token, x) == EOF)
        return 1;

    if (isalpha(x) || x == '_') {
        /* Could be the start of a macro name */
        while (1) {
            /* Read another char */
            errno = 0;
            if ((x = getch(input)) == EOF) {
                if (errno)
                    *err = 1;
                return 1;
            }
            if (!(isalnum(x) || x == '_')) {
                /* Read past the end of the token, so put the char back */
                if (ungetch(input, x) == EOF)
                    return 1;
                break;
            } else {
                /* Store the char */
                if (ungetch(token, x) == EOF)
                    return 1;
            }
        }
    }
    /* Null terminate token */
    if (ungetch(token, '\0') == EOF)
        return 1;
    return 0;
}

int main(int argc, char **argv)
{
    int ret = 0;
    int err;
    /* int x; */
    int j;
    struct buf *input = NULL, *token = NULL;

    if ((input = init_buf()) == NULL) {
        ret = 1;
        goto clean_up;
    }

    if ((token = init_buf()) == NULL) {
        ret = 1;
        goto clean_up;
    }

    if (argc > 1) {
        /* Load command line files into buffer */
        for (j = argc - 1; j; --j)
            if (include(input, *(argv + j))) {
                ret = 1;
                goto clean_up;
            }
    }

    ungetch(input, 'o');
    ungetch(input, 'l');
    ungetch(input, 'l');
    ungetch(input, 'e');
    ungetch(input, 'h');

/*
    while ((x = getch(input)) != EOF)
        putchar(x);
*/
    err = 0;
    while (!getword(token, input, &err))
        printf("%s", token->a);
    if (err) {
        ret = 1;
        goto clean_up;
    }

  clean_up:
    free_buf(input);
    free_buf(token);

    return ret;
}
