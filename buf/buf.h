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

#ifndef BUF_H_
#define BUF_H_

#include <stddef.h>

/* Default gap size */
#define GAP (BUFSIZ - 1)

/* Index to pointer */
#define ITOP(b, i) (i < (size_t) (b->g - b->a) ? \
		b->a + i : b->c + i - (b->g - b->a))
/* Pointer to index */
#define PTOI(b, p) (p < b->g ? p - b->a : p - b->a - (b->c - b->g))

/* Buffer */
struct buffer {
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

typedef struct buffer buf;

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
/* Cursor char */
#define CCH(b) (*b->c)

/* Filename */
#define FN(b) (b->fn)
/* Cursor row number */
#define CRN(b) (b->r)
/* Top of screen row number */
#define TSRN(b) (b->t)
/* Draw start index */
#define DSI(b) (b->d)
/* Mark index */
#define MI(b) (b->m)
/* Mark set */
#define MSET(b) (b->m_set)
/* Modified buffer */
#define BMOD(b) (b->mod)
/* Veritcal centring requested */
#define VCR(b) (b->v)

buf *initbuf(size_t will_use);
void freebuf(buf *b);
int insertch(buf *b, char ch);
int deletech(buf *b);
int leftch(buf *b);
int rightch(buf *b);
int backspacech(buf *b);
size_t homeofline(buf * b);
void endofline(buf *b);
void first(buf *b);
void last(buf *b);
int up(buf *b);
int down(buf *b);
void forwardword(buf *b);
void backwardword(buf *b);
void uppercaseword(buf *b);
void lowercaseword(buf *b);
void forwardpara(buf *b);
void backwardpara(buf *b);
void forwardsent(buf *b);
void backwardsent(buf *b);
int search(buf *b, char *str);
void deletebuf(buf *b);
int buftostr(buf *b, char **str);
void setmark(buf *b);
int killregion(buf *b, char **k, size_t * ks, size_t * kn, int del);
int killfwdline(buf *b, char **k, size_t * ks, size_t * kn);
int uproot(buf *b, char **k, size_t * ks, size_t * kn);
int yank(buf *b, char *k, size_t ks, size_t kn);
int insertfile(buf *b, char *fn);
int save(buf *b);
int matchbrace(buf *b);
void trimwhitespace(buf *b);
int setfilename(buf *b, char *filename);
void pageup(buf *b);
void pagedown(buf *b);

#endif
