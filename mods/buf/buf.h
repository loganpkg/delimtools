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

#ifndef BUF_H
#define BUF_H

struct buf {
    char *a;
    size_t i;
    size_t s;
};

#define get_ch(b, read_stdin) (b->i ? *(b->a + --b->i) \
    : (read_stdin ? getchar() : EOF))

/*
 * put_ch is the same as unget_ch, just a different name depending on the
 * context. put_ch is used for output and unget_ch is used for input.
 */
#define put_ch unget_ch

struct buf *init_buf(size_t init_buf_size);
void free_buf(struct buf *b);
int unget_ch(struct buf *b, int ch);
int include(struct buf *b, char *fn);
void delete_buf(struct buf *b);
int get_word(struct buf *token, struct buf *input, int read_stdin);
int unget_str(struct buf *b, char *str);
int put_str(struct buf *b, char *str);
int put_mem(struct buf *b, char *mem, size_t mem_s);
int buf_dump_buf(struct buf *dst, struct buf *src);

#endif
