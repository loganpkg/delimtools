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

/* Buffer module */

/*
 * References:
 * Section 4.3 External Variables:
 * Brian W. Kernighan and Dennis M. Ritchie, The C Programming Language,
 *     Second Edition, Prentice Hall Software Series, New Jersey, 1988.
 * Clovis L. Tondo and Scott E. Gimpel, The C Answer Book, Second Edition,
 *     PTR Prentice Hall Software Series, New Jersey, 1989.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "../gen/gen.h"
#include "../fs/fs.h"
#include "buf.h"

#define buf_free_size(b) (b->s - b->i)

struct buf *init_buf(size_t init_buf_size)
{
    struct buf *b;
    if ((b = malloc(sizeof(struct buf))) == NULL)
        return NULL;
    if ((b->a = malloc(init_buf_size)) == NULL) {
        free(b);
        return NULL;
    }
    b->s = init_buf_size;
    b->i = 0;
    return b;
}

void free_buf_wrapping(struct buf *b)
{
    /* Just frees the struct, not the mem inside */
    if (b != NULL)
        free(b);
}

void free_buf(struct buf *b)
{
    if (b != NULL) {
        free(b->a);
        free(b);
    }
}

static int grow_buf(struct buf *b, size_t will_use)
{
    char *t;
    size_t new_s;
    /* Gap is big enough, nothing to do */
    if (will_use <= buf_free_size(b))
        return 0;
    if (mof(b->s, 2))
        return 1;
    new_s = b->s * 2;
    if (aof(new_s, will_use))
        return 1;
    new_s += will_use;
    if ((t = realloc(b->a, new_s)) == NULL)
        return 1;
    b->a = t;
    b->s = new_s;
    return 0;
}

int unget_ch(struct buf *b, int ch)
{
    if (b->i == b->s && grow_buf(b, 1))
        return 1;
    *(b->a + b->i++) = ch;
    return 0;
}

int del_ch(struct buf *b)
{
    if (b->i) {
        --b->i;
        return 0;
    }
    return 1;
}

int include(struct buf *b, char *fn)
{
    FILE *fp;
    int x;
    size_t fs;
    size_t back_i;

    if (filesize(fn, &fs))
        return 1;
    if (fs > buf_free_size(b) && grow_buf(b, fs))
        return 1;
    if ((fp = fopen(fn, "rb")) == NULL)
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

int get_word(struct buf *token, struct buf *input, int read_stdin)
{
    /*
     * Deletes the contents of the token buffer, then reads a word from the
     * input buffer and writes it to the token buffer. If read_stdin is
     * non-zero, then stdin will be read when the input buffer is empty.
     * Returns 1 on error and 2 on EOF (with no error).
     */
    int x;
    delete_buf(token);

    /* Always read at least one char */
    errno = 0;
    if ((x = get_ch(input, read_stdin)) == EOF) {
        if (errno)
            return 1;           /* Error */
        return 2;               /* EOF with no error */
    }

    if (put_ch(token, x))
        return 1;               /* Error */

    if (isalpha(x) || x == '_') {
        /* Could be the start of a macro name */
        while (1) {
            /* Read another char */
            errno = 0;
            if ((x = get_ch(input, read_stdin)) == EOF) {
                if (errno)
                    return 1;   /* Error */
                return 2;       /* EOF with no error */
            }
            if (!(isalnum(x) || x == '_')) {
                /* Read past the end of the token, so put the char back */
                if (unget_ch(input, x))
                    return 1;
                break;
            } else {
                /* Store the char */
                if (put_ch(token, x))
                    return 1;
            }
        }
    }
    /* Null terminate token */
    if (put_ch(token, '\0'))
        return 1;

    return 0;
}

int unget_str(struct buf *b, char *str)
{
    size_t len;
    if (str == NULL)
        return 1;
    if (!(len = strlen(str)))
        return 0;
    if (len > buf_free_size(b) && grow_buf(b, len))
        return 1;
    while (len)
        *(b->a + b->i++) = *(str + --len);
    return 0;
}

int put_str(struct buf *b, char *str)
{
    size_t len;
    if (str == NULL)
        return 1;
    if (!(len = strlen(str)))
        return 0;
    if (len > buf_free_size(b) && grow_buf(b, len))
        return 1;
    memcpy(b->a + b->i, str, len);
    b->i += len;
    return 0;
}

int put_mem(struct buf *b, char *mem, size_t mem_s)
{
    if (mem == NULL)
        return 1;
    if (!mem_s)
        return 0;
    if (mem_s > buf_free_size(b) && grow_buf(b, mem_s))
        return 1;
    memcpy(b->a + b->i, mem, mem_s);
    b->i += mem_s;
    return 0;
}

int buf_dump_buf(struct buf *dst, struct buf *src)
{
    if (src->i > buf_free_size(dst) && grow_buf(dst, src->i))
        return 1;
    memcpy(dst->a + dst->i, src->a, src->i);
    dst->i += src->i;
    src->i = 0;
    return 0;
}

int write_buf_details(FILE * fp, void *x)
{
    /* Write details to be called via a function pointer in atomic_write */
    struct buf *b = x;

    if (fwrite(b->a, 1, b->i, fp) != b->i)
        return 1;

    return 0;
}

int write_buf(struct buf *b, char *fn)
{
    return atomic_write(fn, b, write_buf_details);
}
