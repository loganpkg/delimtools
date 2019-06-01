/*
 * Copyright (c) 2019 Logan Ryan McLintock
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
 * spot text editor.
 * Dedicated to my son who was just a "spot" in his first ultrasound.
 * To install
 * cc -ansi -O2 -g -Wall -Wextra -pedantic -o spot spot.c -lncurses
 * then place spot somewhere in your PATH.
 */

struct buf {
  char *fn; /* Filename */
  char *a; /* Array */
  char *g; /* Start of gap */
  char *c; /* Cursor */
  char *m; /* Mark */
  size_t s; /* Size of array */
  size_t r; /* Cursor's row number */
  size_t t; /* Row number of top of screen */
  int m_set; /* If the mark is set */
  int mod; /* If the buffer is modified */
};
