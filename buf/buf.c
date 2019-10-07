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

#define _GNU_SOURCE

#include <sys/stat.h>
#include <ctype.h>
#include <curses.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

struct buf *initbuf(size_t will_use)
{
	struct buf *b = NULL;
	size_t s;

	if ((b = calloc(1, sizeof(struct buf))) == NULL) {
		LOG("calloc failed");
		return NULL;
	}

	if (safeadd(&s, 3, GAP, 1, will_use)) {
		LOG("safeadd failed");
		return NULL;
	}

	b->a = NULL;
	if ((b->a = malloc(s)) == NULL) {
		LOG("malloc failed");
		return NULL;
	}

	b->g = b->a;
	*(b->c = b->a + s - 1) = '~';
	b->s = s;

	return b;
}

void freebuf(struct buf *b)
{
	if (b != NULL) {
		free(b->fn);
		b->fn = NULL;
		free(b->a);
		b->a = NULL;
		free(b);
		b = NULL;
	}
}

static int growgap(struct buf *b, size_t will_use)
{
	char *new_a = NULL;
	size_t new_s, gap_s, non_gap_s, s_increase;
	size_t g_offset = b->g - b->a;
	size_t c_offset = b->c - b->a;

	gap_s = b->c - b->g;

	if (will_use <= gap_s)
		return 0;

	non_gap_s = b->s - gap_s;

	if (safeadd(&new_s, 3, non_gap_s, will_use, GAP)) {
		LOG("safeadd failed");
		return -1;
	}

	s_increase = new_s - b->s;

	if ((new_a = realloc(b->a, new_s)) == NULL) {
		LOG("realloc failed");
		return -1;
	}

	memmove(new_a + c_offset + s_increase, new_a + c_offset,
		b->s - c_offset);

	b->a = new_a;
	b->g = new_a + g_offset;
	b->c = new_a + c_offset + s_increase;
	b->s = new_s;

	/* Indexes do not need updating */

	return 0;
}

int insertch(struct buf *b, char ch)
{
	if (b->g == b->c) {
		if (growgap(b, 1)) {
			return -1;
		}
	}

	b->m_set = 0;
	b->mod = 1;

	if (ch == '\n')
		++b->r;

	*(b->g++) = ch;

	return 0;
}

int deletech(struct buf *b)
{
	if (b->c == b->a + b->s - 1)
		return -1;

	b->m_set = 0;
	b->mod = 1;

	return *(b->c++);
}

int leftch(struct buf *b)
{
	if (b->g == b->a)
		return -1;

	*(--b->c) = *(--b->g);

	if (*b->c == '\n')
		--b->r;

	return *b->c;
}

int rightch(struct buf *b)
{
	if (b->c == b->a + b->s - 1)
		return -1;

	if (*b->c == '\n')
		++b->r;

	*(b->g++) = *(b->c++);
	return *b->c;
}

int backspacech(struct buf *b)
{
	if (leftch(b) == -1)
		return -1;

	return deletech(b);
}

size_t home(struct buf * b)
{
	size_t count = 0;
	int x;
	while ((x = leftch(b)) != -1) {
		if (x == '\n') {
			rightch(b);
			break;
		}
		++count;
	}

	return count;
}

void end(struct buf *b)
{
	int x;

	if (*b->c == '\n')
		return;

	while ((x = rightch(b)) != -1) {
		if (x == '\n')
			break;
	}
}

void first(struct buf *b)
{
	size_t gap_s;

	if (b->g == b->a)
		return;

	gap_s = b->c - b->g;

	b->r = 0;

	memmove(b->a + gap_s, b->a, b->g - b->a);

	b->g = b->a;
	b->c = b->a + gap_s;
}

void last(struct buf *b)
{
	char *end_of_buf = b->a + b->s - 1;

	if (b->c == end_of_buf)
		return;

	while (b->c != end_of_buf) {
		if (*b->c == '\n')
			++b->r;

		*(b->g++) = *(b->c++);
	}
}

int up(struct buf *b)
{
	size_t count;

	if (!b->r)
		return -1;

	count = home(b);

	leftch(b);

	home(b);

	if (*b->c == '\n')
		return *b->c;

	while (count) {
		if (rightch(b) == '\n')
			break;

		--count;
	}
	return *b->c;
}

int down(struct buf *b)
{
	size_t count;
	int x;

	count = home(b);

	end(b);

	if (rightch(b) == -1) {
		home(b);
		return -1;
	}

	if (*b->c == '\n')
		return *b->c;

	while (count) {
		x = rightch(b);
		if (x == '\n' || x == -1)
			break;

		--count;
	}
	return *b->c;
}

void forwardword(struct buf *b)
{

	while (!isalnum(*b->c))
		if (rightch(b) == -1)
			break;

	while (isalnum(*b->c))
		if (rightch(b) == -1)
			break;
}

void backwardword(struct buf *b)
{

	while (leftch(b) != -1)
		if (isalnum(*b->c))
			break;

	while (leftch(b) != -1)
		if (!isalnum(*b->c))
			break;

	if (b->g != b->a)
		rightch(b);
}

void uppercaseword(struct buf *b)
{

	/* Advance to start of next word */
	while (!isalpha(*b->c))
		if (rightch(b) == -1)
			break;

	/* Swap letters to uppercase */
	while (isalpha(*b->c)) {
		*b->c = toupper(*b->c);
		if (rightch(b) == -1)
			break;
	}
}

void lowercaseword(struct buf *b)
{

	/* Advance to start of next word */
	while (!isalpha(*b->c))
		if (rightch(b) == -1)
			break;

	/* Swap letters to lowercase */
	while (isalpha(*b->c)) {
		*b->c = tolower(*b->c);
		if (rightch(b) == -1)
			break;
	}
}

void forwardpara(struct buf *b)
{
	while (isspace(*b->c))
		if (rightch(b) == -1)
			break;

	while (rightch(b) != -1) {
		if (*b->c == '\n') {
			if (rightch(b) == -1)
				break;
			if (*b->c == '\n')
				break;
		}
	}
}

void backwardpara(struct buf *b)
{
	while (isspace(*b->c))
		if (leftch(b) == -1)
			break;

	while (leftch(b) != -1) {
		if (*b->c == '\n') {
			if (leftch(b) == -1)
				break;
			if (*b->c == '\n')
				break;
		}
	}

	if (b->g != b->a)
		rightch(b);
}

void forwardsent(struct buf *b)
{
	forwardpara(b);
	leftch(b);
}

void backwardsent(struct buf *b)
{
	leftch(b);
	backwardpara(b);
	while (isspace(*b->c))
		if (rightch(b) == -1)
			break;
}

int search(struct buf *b, char *str)
{
	char *p = NULL;

	if (str == NULL || b->c == b->a + b->s - 1)
		return -1;

	if ((p = memmem(b->c + 1, b->s - 1 - (b->c - b->a + 1), str,
		    strlen(str))) == NULL)
		return -1;

	while (b->c != p)
		rightch(b);

	return 0;
}

void deletebuf(struct buf *b)
{
	b->g = b->a;
	b->c = b->a + b->s - 1;
	b->m = 0;
	b->r = 0;
	b->t = 0;
	b->d = 0;
	b->mr = 0;
	b->m_set = 0;
	b->mod = 1;
	b->v = 0;
}

int buftostr(struct buf *b, char **str)
{
	char *p, *q;
	free(*str);
	*str = NULL;

	if ((*str = malloc(b->s - (b->c - b->g))) == NULL) {
		LOG("malloc failed");
		return -1;
	}

	q = *str;
	/* Before gap */
	for (p = b->a; p < b->g; ++p) {
		if (*p != '\0') {
			*q = *p;
			++q;
		}
	}
	/* After gap */
	for (p = b->c; p < b->a + b->s - 2; ++p) {
		if (*p != '\0') {
			*q = *p;
			++q;
		}
	}
	*q = '\0';
	return 0;
}

void setmark(struct buf *b)
{
	/*
	 * The mark index does not need to be updated for inserts
	 * and deletes as the mark is unset if these occur.
	 */

	b->m_set = 1;
	b->m = CI(b);
	b->mr = b->r;
}

int killregion(struct buf *b, char **k, size_t * ks, size_t * kn, int del)
{
	if (!b->m_set || CI(b) == b->m) {
		b->m_set = 0;
		return 0;
	}

	*kn = b->r > b->mr ? b->r - b->mr : b->mr - b->r;

	free(*k);
	*k = NULL;

	if (CI(b) < b->m) {
		/* Cursor, then mark */
		*ks = b->m - CI(b);

		if ((*k = malloc(*ks)) == NULL) {
			LOG("malloc failed");
			return -1;
		}

		memcpy(*k, b->c, *ks);

		if (del)
			b->c = ITOP(b, b->m);

	} else {
		/* Mark, then cursor */
		*ks = CI(b) - b->m;

		if ((*k = malloc(*ks)) == NULL) {
			LOG("malloc failed");
			return -1;
		}

		memcpy(*k, ITOP(b, b->m), *ks);

		if (del) {
			b->g = ITOP(b, b->m);
			b->r -= *kn;
		}
	}

	b->m_set = 0;
	b->m = 0;

	if (del)
		b->mod = 1;

	return 0;
}

int killfwdline(struct buf *b, char **k, size_t * ks, size_t * kn)
{
	if (*b->c == '\n') {
		deletech(b);
		return 0;
	}

	setmark(b);
	end(b);

	if (killregion(b, k, ks, kn, 1)) {
		LOG("killregion failed");
		return -1;
	}

	return 0;
}

int uproot(struct buf *b, char **k, size_t * ks, size_t * kn)
{
	setmark(b);
	home(b);

	if (killregion(b, k, ks, kn, 1)) {
		LOG("killregion failed");
		return -1;
	}

	return 0;
}

int yank(struct buf *b, char *k, size_t ks, size_t kn)
{
	if (growgap(b, ks)) {
		LOG("growgap failed");
		return -1;
	}

	memcpy(b->g, k, ks);

	b->g += ks;
	b->r += kn;

	b->m_set = 0;
	b->m = 0;
	b->mod = 1;

	return 0;
}

int insertfile(struct buf *b, char *fn)
{
	int ret = 0;
	size_t fs;
	FILE *fp = NULL;

	if (filesize(&fs, fn)) {
		LOG("filesize failed");
		return -1;
	}

	if ((fp = fopen(fn, "r")) == NULL) {
		LOG("fopen failed");
		return -1;
	}

	if (growgap(b, fs)) {
		LOG("growgap failed");
		ret = -1;
		goto clean_up;
	}

	if (fread(b->c - fs, 1, fs, fp) != fs) {
		LOG("fread failed");
		ret = -1;
		goto clean_up;
	}

	if (ferror(fp)) {
		LOG("fread error");
		ret = -1;
		goto clean_up;
	}

 clean_up:
	if (fclose(fp)) {
		LOG("fclose failed");
		ret = -1;
	}

	if (!ret) {
		b->c -= fs;
		b->m_set = 0;
		b->m = 0;
		b->mod = 1;
	}

	return ret;
}

int save(struct buf *b)
{
	int ret = 0;
	FILE *fp = NULL;
	size_t s_before_gap, s_after_gap;

	if (!b->mod)
		return 0;

	if (b->fn == NULL)
		return -1;

	if ((fp = fopen(b->fn, "w")) == NULL) {
		LOG("fopen failed");
		return -1;
	}

	/* Before gap */
	s_before_gap = b->g - b->a;
	if (fwrite(b->a, 1, s_before_gap, fp) != s_before_gap) {
		LOG("fwrite failed before gap");
		ret = -1;
		goto clean_up;
	}

	/* After gap */
	s_after_gap = b->s - 1 - (b->c - b->a);
	if (fwrite(b->c, 1, s_after_gap, fp) != s_after_gap) {
		LOG("fwrite failed after gap");
		ret = -1;
		goto clean_up;
	}

 clean_up:
	if (fclose(fp)) {
		LOG("fclose failed");
		ret = -1;
	}

	if (!ret)
		b->mod = 0;

	return ret;
}

int matchbrace(struct buf *b)
{
	int (*move) (struct buf *);
	char original;
	char target;
	int fwd;
	int x;
	size_t depth;
	int found;

	fwd = 0;
	original = *b->c;
	switch (original) {
	case '}':
		target = '{';
		break;
	case ')':
		target = '(';
		break;
	case ']':
		target = '[';
		break;
	case '{':
		target = '}';
		fwd = 1;
		break;
	case '(':
		target = ')';
		fwd = 1;
		break;
	case '[':
		target = ']';
		fwd = 1;
		break;
	default:
		return -1;
	}

	if (fwd)
		move = &rightch;
	else
		move = &leftch;

	depth = 0;
	found = 0;
	while ((x = (*move) (b)) != -1) {
		if (x == original) {
			++depth;
		} else if (x == target) {
			if (!depth) {
				found = 1;
				break;
			} else {
				--depth;
			}
		}
	}

	if (!found)
		return -1;
	else
		return 0;
}

void trimwhitespace(struct buf *b)
{
	int x;
	int line_feed;
	int end_of_line;
	size_t loc_index;
	size_t del_count = 0;

	loc_index = CI(b);
	last(b);

	/* Trim end of buffer */
	line_feed = 0;
	while ((x = leftch(b)) != -1) {
		if (x == '\n') {
			if (!line_feed) {
				line_feed = 1;
			} else {
				deletech(b);
				if (CI(b) < loc_index)
					++del_count;
			}
		} else if (x == ' ' || x == '\t') {
			deletech(b);
			if (CI(b) < loc_index)
				++del_count;
		} else {
			break;
		}
	}

	/* Trim body */
	end_of_line = 0;
	while ((x = leftch(b)) != -1) {
		switch (x) {
		case '\n':
			end_of_line = 1;
			break;
		case ' ':
		case '\t':
			if (end_of_line) {
				deletech(b);
				if (CI(b) < loc_index)
					++del_count;
			}
			break;
		default:
			end_of_line = 0;
		}
	}

	/* Move back to original location */
	loc_index -= del_count;

	if (loc_index) {
		while (rightch(b) != -1) {
			if (CI(b) == loc_index)
				break;
		}
	}
}

int setfilename(struct buf *b, char *filename)
{
	char *new_fn = NULL;
	size_t len, res;

	if (filename == NULL) {
		free(b->fn);
		b->fn = NULL;
		return 0;
	}

	len = strlen(filename);

	if (!len) {
		free(b->fn);
		b->fn = NULL;
		return 0;
	}

	if (safeadd(&res, 2, len, 1)) {
		LOG("safeadd failed");
		return -1;
	}

	if ((new_fn = malloc(res)) == NULL) {
		LOG("malloc failed");
		return -1;
	}

	memcpy(new_fn, filename, res);

	free(b->fn);
	b->fn = new_fn;
	b->mod = 1;
	return 0;
}

void pageup(struct buf *b)
{
	int h, th;
	h = getmaxy(stdscr);
	if (h < 3)
		return;

	/*
	 * Height of text portion of the screen.
	 * No adjustment is made for line wrapping.
	 */
	th = h - 2;

	while (th && up(b) != -1)
		--th;
}

void pagedown(struct buf *b)
{
	int h, th;
	h = getmaxy(stdscr);
	if (h < 3)
		return;

	/*
	 * Height of text portion of the screen.
	 * No adjustment is made for line wrapping.
	 */
	th = h - 2;

	while (th && down(b) != -1)
		--th;
}
