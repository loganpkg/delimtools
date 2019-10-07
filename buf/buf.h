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
 * buf library.
 */

#include <stddef.h>

/* Default gap size */
#define GAP (BUFSIZ - 1)

/* Index to pointer */
#define ITOP(b, i) (i < (size_t) (b->g - b->a) ? \
		b->a + i : b->c + i - (b->g - b->a))
/* Pointer to index */
#define PTOI(b, p) (p < b->g ? p - b->a : p - b->a - (b->c - b->g))

/* Buffer */
struct buf {
	char *fn;		/* Filename */
	char *a;		/* Array */
	char *g;		/* Start of gap */
	char *c;		/* Cursor */
	size_t s;		/* Size of array */
	size_t r;		/* Cursor row number */
	size_t t;		/* Top of screen row number */
	size_t d;		/* Draw start index */
	size_t m;		/* Mark index */
	size_t mr;		/* Mark row number */
	int m_set;		/* Mark set */
	int mod;		/* Modified buffer */
	int v;			/* Veritcal centring requested */
};

/*
 * Gap buffer structure
 *    a                       g               c
 * p 133 134 135 136 137 138 139 140 141 142 143 144 145 146 147
 *  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *  | b | u | f | _ | r | o | X | X | X | X | c | k | s | ! | ~ | s = 15
 *  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * i  0   1   2   3   4   5                   6   7   8   9  10
 *                                            CI             EI
 */

/* Read a character */
#define READ(b, i) (*ITOP(b, i))
/* Cursor index */
#define CI(b) ((size_t) (b->g - b->a))
/* End index */
#define EI(b) (b->s - 1 - (b->c - b->g))

struct buf *initbuf(size_t will_use);
void freebuf(struct buf *b);
int insertch(struct buf *b, char ch);
int deletech(struct buf *b);
int leftch(struct buf *b);
int rightch(struct buf *b);
int backspacech(struct buf *b);
size_t home(struct buf * b);
void end(struct buf *b);
void first(struct buf *b);
void last(struct buf *b);
int up(struct buf *b);
int down(struct buf *b);
void forwardword(struct buf *b);
void backwardword(struct buf *b);
void uppercaseword(struct buf *b);
void lowercaseword(struct buf *b);
void forwardpara(struct buf *b);
void backwardpara(struct buf *b);
void forwardsent(struct buf *b);
void backwardsent(struct buf *b);
int search(struct buf *b, char *str);
void deletebuf(struct buf *b);
int buftostr(struct buf *b, char **str);
void setmark(struct buf *b);
int killregion(struct buf *b, char **k, size_t * ks, size_t * kn, int del);
int killfwdline(struct buf *b, char **k, size_t * ks, size_t * kn);
int uproot(struct buf *b, char **k, size_t * ks, size_t * kn);
int yank(struct buf *b, char *k, size_t ks, size_t kn);
int insertfile(struct buf *b, char *fn);
int save(struct buf *b);
int matchbrace(struct buf *b);
void trimwhitespace(struct buf *b);
int setfilename(struct buf *b, char *filename);
void pageup(struct buf *b);
void pagedown(struct buf *b);
