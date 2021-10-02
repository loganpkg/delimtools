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

#ifndef GAPBUF_H
#define GAPBUF_H

/* gap buffer */
struct gapbuf {
    struct gapbuf *prev;        /* Previous gap buffer node */
    char *fn;                   /* Filename */
    char *a;                    /* Start of gap buffer */
    char *g;                    /* Start of gap */
    char *c;                    /* Cursor (to the right of the gap) */
    char *e;                    /* End of gap buffer */
    size_t r;                   /* Row number (starting from 1) */
    size_t sc;                  /* Sticky column number (starting from 0) */
    int sc_set;                 /* Sticky column is set */
    size_t d;                   /* Draw start index (ignores the gap) */
    size_t m;                   /* Mark index (ignores the gap) */
    size_t mr;                  /* Row number at the mark */
    int m_set;                  /* Mark is set */
    int mod;                    /* Gap buffer text has been modified */
    struct gapbuf *next;        /* Next gap buffer node */
};

/* Converts the cursor pointer to an index */
#define CURSOR_INDEX(b) ((size_t) (b->g - b->a))

/* Converts an index to a pointer */
#define INDEX_TO_POINTER(b, i) (b->a + b->i < b->g ? b->a + b->i \
    : b->c + b->i - (b->g - b->a))

/* Delete gap buffer */
#define DELETEGAPBUF(b) do {b->g = b->a; b->c = b->e; b->r = 1; b->d = 0; \
    b->m = 0; b->mr = 1; b->m_set = 0; b->mod = 1;} while (0)

struct gapbuf *init_gapbuf(size_t init_gapbuf_size);
void free_gapbuf_list(struct gapbuf *b);
int insert_ch(struct gapbuf *b, char c, size_t mult);
int delete_ch(struct gapbuf *b, size_t mult);
int backspace_ch(struct gapbuf *b, size_t mult);
int left_ch(struct gapbuf *b, size_t mult);
int right_ch(struct gapbuf *b, size_t mult);
void start_of_gapbuf(struct gapbuf *b);
void end_of_gapbuf(struct gapbuf *b);
void start_of_line(struct gapbuf *b);
void end_of_line(struct gapbuf *b);
size_t col_num(struct gapbuf *b);
int up_line(struct gapbuf *b, size_t mult);
int down_line(struct gapbuf *b, size_t mult);
void forward_word(struct gapbuf *b, int mode, size_t mult);
void backward_word(struct gapbuf *b, size_t mult);
void trim_clean(struct gapbuf *b);
void str_gapbuf(struct gapbuf *b);
void set_mark(struct gapbuf *b);
void clear_mark(struct gapbuf *b);
int forward_search(struct gapbuf *b, char *p, size_t n);
int regex_forward_search(struct gapbuf *b, char *find, int nl_insen);
void switch_cursor_and_mark(struct gapbuf *b);
char *region_to_str(struct gapbuf *b);
int regex_replace_region(struct gapbuf *b, char *dfdr, int nl_insen);
int match_bracket(struct gapbuf *b);
int copy_region(struct gapbuf *b, struct gapbuf *p);
int delete_region(struct gapbuf *b);
int cut_region(struct gapbuf *b, struct gapbuf *p);
int insert_str(struct gapbuf *b, char *str, size_t mult);
int paste(struct gapbuf *b, struct gapbuf *p, size_t mult);
int cut_to_eol(struct gapbuf *b, struct gapbuf *p);
int cut_to_sol(struct gapbuf *b, struct gapbuf *p);
int insert_file(struct gapbuf *b, char *fn);
int write_file(struct gapbuf *b);

#endif
