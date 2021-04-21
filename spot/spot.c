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

/* No bound or gap size checks are performed */
#define INSERTCH(b, x) *b->g++ = x; if (x == '\n') ++b->r
#define DELETECH(b) ++b->c
#define BACKSPACECH(b) if (*--b->g == '\n') --b->r
#define LEFTCH(b) *--b->c = *--b->g; if (*b->c == '\n') --b->r
#define RIGHTCH(b) if (*b->c == '\n') ++b->r; *b->g++ = *b->c++

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
