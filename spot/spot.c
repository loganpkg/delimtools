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

/*
 * spot: text editor.
 * Dedicated to my son who was only a 4mm "spot" in his first ultrasound.
 */

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Initial buffer size */
#define INIT_BUF_SIZE BUFSIZ

/* size_t Addition OverFlow test */
#define AOF(a, b) a > SIZE_MAX - b
/* size_t Multiplication OverFlow test */
#define MOF(a, b) a && b > SIZE_MAX / a

/* No bound or gap size checks are performed */
#define INSERTCH(b, x) *b->g++ = x; if (x == '\n') ++b->r
#define DELETECH(b) ++b->c
#define BACKSPACECH(b) if (*--b->g == '\n') --b->r
#define LEFTCH(b) *--b->c = *--b->g; if (*b->c == '\n') --b->r
#define RIGHTCH(b) if (*b->c == '\n') ++b->r; *b->g++ = *b->c++

/* Update settings when a buffer is modified */
#define BUFMOD b->m = 0; b->m_set = 0; b->mod = 1

/* gap buffer */
struct buf {
    char *fn;                   /* Filename */
    char *a;                    /* Start of buffer */
    char *g;                    /* Start of gap */
    char *c;                    /* Cursor (to the right of the gap) */
    char *e;                    /* End of buffer */
    size_t r;                   /* Row number (starting from 1) */
    size_t d;                   /* Draw start index (ignores the gap) */
    size_t m;                   /* Mark index (ignores the gap) */
    int m_set;                  /* Mark is set */
    int mod;                    /* Buffer text has been modified */
};

struct buf *init_buf(void)
{
    /* Initialises a buffer */
    struct buf *b;
    if ((b = malloc(sizeof(struct buf))) == NULL)
        return NULL;
    if ((b->a = malloc(INIT_BUF_SIZE)) == NULL)
        return NULL;
    b->e = b->a + INIT_BUF_SIZE - 1;
    b->g = b->a;
    b->c = b->e;
    *b->e = '~';                /* End of buffer char. Cannot be deleted. */
    b->r = 1;
    b->d = 0;
    b->m = 0;
    b->m_set = 0;
    b->mod = 0;
    return b;
}

void free_buf(struct buf *b)
{
    /* Frees a buffer */
    if (b != NULL) {
        free(b->a);
        free(b);
    }
}

int grow_gap(struct buf *b, size_t will_use)
{
    /* Grows the gap size of a buffer */
    char *t;
    size_t buf_size = b->e - b->a + 1;
    if (MOF(buf_size, 2))
        return 1;
    buf_size *= 2;
    if (AOF(buf_size, will_use))
        return 1;
    buf_size += will_use;
    if ((t = malloc(buf_size)) == NULL)
        return 1;
    /* Copy text before the gap */
    memcpy(t, b->a, b->g - b->a);
    /* Copy text after the gap */
    memcpy(t + buf_size - 1 - (b->e - b->c + 1), b->c, b->e - b->c + 1);
    /* Update pointers, indices do not need to be changed */
    b->g = t + (b->g - b->a);
    b->c = t + buf_size - 1 - (b->e - b->c + 1);
    b->e = t + buf_size - 1;
    /* Free old memory */
    free(b->a);
    b->a = t;
    return 0;
}

int insert_ch(struct buf *b, char c, size_t mult)
{
    /* Inserts a char mult times into the buffer */
    if ((size_t) (b->c - b->g) < mult)
        if (grow_gap(b, mult))
            return 1;
    while (mult--)
        INSERTCH(b, c);
    BUFMOD;
    return 0;
}

int delete_ch(struct buf *b, size_t mult)
{
    /* Deletes mult chars in a buffer */
    if (mult > (size_t) (b->e - b->c))
        return 1;
    while (mult--)
        DELETECH(b);
    BUFMOD;
    return 0;
}

int backspace_ch(struct buf *b, size_t mult)
{
    /* Backspaces mult chars in a buffer */
    if (mult > (size_t) (b->g - b->a))
        return 1;
    while (mult--)
        BACKSPACECH(b);
    BUFMOD;
    return 0;
}

int left_ch(struct buf *b, size_t mult)
{
    /* Move the cursor left mult positions */
    if (mult > (size_t) (b->g - b->a))
        return 1;
    while (mult--)
        LEFTCH(b);
    return 0;
}

int right_ch(struct buf *b, size_t mult)
{
    /* Move the cursor right mult positions */
    if (mult > (size_t) (b->e - b->c))
        return 1;
    while (mult--)
        RIGHTCH(b);
    return 0;
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

int insert_file(struct buf *b, char *fn)
{
    /* Inserts a file into the righthand side of the gap */
    size_t fs;
    FILE *fp;
    if (filesize(fn, &fs))
        return 1;
    if (fs > (size_t) (b->c - b->g))
        if (grow_gap(b, fs))
            return 1;
    if ((fp = fopen(fn, "r")) == NULL)
        return 1;
    if (fread(b->c - fs, 1, fs, fp) != fs) {
        fclose(fp);
        return 1;
    }
    b->c -= fs;
    BUFMOD;
    return 0;
}

int write_file(struct buf *b)
{
    /* Writes a buffer to file */
    char *tmp_fn;
    FILE *fp;
    size_t n;

    if (asprintf(&tmp_fn, "%s~", b->fn) == -1)
        return 1;
    if ((fp = fopen(tmp_fn, "w")) == NULL) {
        free(tmp_fn);
        return 1;
    }
    /* Before gap */
    n = b->g - b->a;
    if (fwrite(b->a, 1, n, fp) != n) {
        free(tmp_fn);
        fclose(fp);
        return 1;
    }
    /* After gap, excluding the last character */
    n = b->e - b->c;
    if (fwrite(b->c, 1, n, fp) != n) {
        free(tmp_fn);
        fclose(fp);
        return 1;
    }
    if (fclose(fp)) {
        free(tmp_fn);
        return 1;
    }
    if (rename(tmp_fn, b->fn)) {
        free(tmp_fn);
        return 1;
    }
    free(tmp_fn);
    b->mod = 0;
    return 0;
}
