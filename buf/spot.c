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
 */

#define _GNU_SOURCE

#include <sys/stat.h>
#include <curses.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Default gap size */
#define GAP (BUFSIZ - 1)

/* Index to pointer */
#define ITOP(b, i)   (i < (size_t) (b->g - b->a) ? b->a + i : b->c + i - (b->g - b->a))
/* Pointer to index */
#define PTOI(b, p)   (p < b->g ? p - b->a : p - b->a - (b->c - b->g))

struct buf {
	char *fn;		/* Filename */
	char *a;		/* Array */
	char *g;		/* Start of gap */
	char *c;		/* Cursor */
	size_t s;		/* Size of array */
	size_t r;		/* Cursor row number */
	size_t t;		/* Top of screen row number (text editor) */
	size_t d;		/* Draw start index (text editor) */
	size_t m;		/* Mark index */
	size_t mr;		/* Mark row number */
	int m_set;		/* Mark set */
	int mod;		/* Modified buffer */
	int v;			/* Veritcal centring requested (text editor) */
};

/*
 * Gap buffer structure
 *    a                       g               c              s-1
 * p 133 134 135 136 137 138 139 140 141 142 143 144 145 146 147
 *  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *  | s | n | a | g | _ | r | X | X | X | X | o | c | k | s | ~ | s = 15
 *  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * i  0   1   2   3   4   5                   6   7   8   9  10
 *                                            CI             EI
 */

/* Error message */
#define LOG(m) fprintf(stderr, "%s:%d: error: " m "\n", __FILE__, __LINE__)
/* size_t addtion overflow test */
#define ADDOF(a, b)  ((a) > SIZE_MAX - (b))
/* size_t multiplication overflow test */
#define MULTOF(a, b) ((a) && (b) > SIZE_MAX / (a))

/* Read a character */
#define READ(b, i) (*ITOP(b, i))
/* Cursor index */
#define CI(b) ((size_t) (b->g - b->a))
/* End index */
#define EI(b) (b->s - 1 - (b->c - b->g))

#define Cspc 0
#define Ca 1
#define Cb 2
#define Cc 3
#define Cd 4
#define Ce 5
#define Cf 6
#define Cg 7
#define Ch 8
#define Ci 9
#define Cj 10
#define Ck 11
#define Cl 12
#define Cm 13
#define Cn 14
#define Co 15
#define Cp 16
#define Cq 17
#define Cr 18
#define Cs 19
#define Ct 20
#define Cu 21
#define Cv 22
#define Cw 23
#define Cx 24
#define Cy 25
#define Cz 26
#define ESC 27
#define Cqm 127

struct ed {
	struct buf **t;		/* Array of text buffers */
	size_t s;		/* Size of the text buffer array */
	size_t ab;		/* Active text buffer */
	char *k;		/* Kill array */
	size_t ks;		/* Size of kill array */
	size_t kn;		/* Number of \n characters in kill array */
	struct buf *cl;		/* Command line buffer */
	char *cl_str;		/* Command line string */
	char *search_str;	/* Search string */
	int cl_active;		/* Editing is in the command line */
	int operation;		/* Operation that requires the command line */
	int in_ret;		/* Return value of internal operation */
	int running;		/* Editor is running */
};

int safeadd(size_t * res, int num_args, ...)
{
	va_list ap;
	int i;
	size_t total = 0;
	size_t arg_val = 0;

	va_start(ap, num_args);
	for (i = 0; i < num_args; ++i) {
		arg_val = va_arg(ap, size_t);
		if (ADDOF(total, arg_val)) {
			LOG("integer overflow");
			va_end(ap);
			return -1;
		}
		total += arg_val;
	}
	va_end(ap);
	*res = total;
	return 0;
}

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

int search(struct buf *b, char *str)
{
	char *p = NULL;

	if (str == NULL || b->c == b->a + b->s - 1) {
		return -1;
	}

	if ((p =
	     memmem(b->c + 1, b->s - 1 - (b->c - b->a + 1), str,
		    strlen(str))) == NULL) {
		return -1;
	}

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

int filesize(size_t * fs, char *fn)
{
	struct stat st;

	if (fn == NULL || !strlen(fn)) {
		LOG("empty filename");
		return -1;
	}

	if (stat(fn, &st)) {
		LOG("stat failed");
		return -1;
	}

	if (!S_ISREG(st.st_mode)) {
		LOG("not regular file");
		return -1;
	}

	if (st.st_size < 0) {
		LOG("negative file size");
		return -1;
	}

	*fs = (size_t) st.st_size;
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

	last(b);

	/* Trim end of buffer */
	line_feed = 0;
	while ((x = leftch(b)) != -1) {
		if (x == '\n') {
			if (!line_feed) {
				line_feed = 1;
			} else {
				deletech(b);
			}
		} else if (x == ' ' || x == '\t') {
			deletech(b);
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
			}
			break;
		default:
			end_of_line = 0;
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

	/* Height of text portion of the screen */
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

	/* Height of text portion of the screen */
	th = h - 2;

	while (th && down(b) != -1)
		--th;
}

int hexnum(int *h, int x)
{
	if (!isxdigit(x))
		return -1;

	if (isdigit(x))
		*h = x - '0';
	else if (islower(x))
		*h = x - 'a' + 10;
	else
		*h = x - 'A' + 10;

	return 0;
}

int inserthex(struct buf *b)
{
	int x0, x1, h0, h1;

	x0 = getch();
	if (hexnum(&h0, x0))
		return -1;

	x1 = getch();
	if (hexnum(&h1, x1))
		return -1;

	if (insertch(b, h0 * 16 + h1)) {
		LOG("insertch failed");
		return -1;
	}

	return 0;
}

int initnc(void)
{
	int ret = 0;
	WINDOW *w = NULL;

	if ((w = initscr()) == NULL) {
		LOG("initscr failed");
		return -1;
	}

	if (raw() == ERR) {
		LOG("raw failed");
		ret = -1;
		goto clean_up;
	}

	if (noecho() == ERR) {
		LOG("noecho failed");
		ret = -1;
		goto clean_up;
	}

	if (keypad(stdscr, TRUE) == ERR) {
		LOG("keypad failed");
		ret = -1;
		goto clean_up;
	}

 clean_up:
	if (ret) {
		if (endwin() == ERR)
			LOG("endwin failed");
	}

	return ret;
}

int freenc(void)
{
	if (endwin() == ERR) {
		LOG("endwin failed");
		return -1;
	}

	return 0;
}

void level(struct buf *b)
{
	if (clear() == ERR)
		LOG("clear failed");

	b->v = 1;
}

void centre(struct buf *b, int th)
{
	size_t row;

	/* Max row index where centring will result in t being set to zero */
	size_t t_zero_range;

	/* If text portion screen height is odd */
	if (th % 2) {
		t_zero_range = th / 2;
	} else {
		t_zero_range = th / 2 - 1;
	}

	/* If near top of buffer */
	if (b->r <= t_zero_range) {
		b->t = 0;
	} else {
		b->t = b->r - t_zero_range;
	}

	if (!b->t || !CI(b)) {
		b->d = 0;
		b->v = 0;
		return;
	}

	/* Search backwards to find the start of row t */
	b->d = CI(b) - 1;
	row = b->r;
	while (b->d) {
		if (READ(b, b->d) == '\n') {
			--row;
			if (row == b->t - 1) {
				++b->d;
				break;
			}
		}

		--b->d;
	}

	b->v = 0;
}

void drawbuf(struct buf *b, int *cp_set, int *cy, int *cx, int cursor_start)
{
	size_t i;
	char ch;
	int hl_on;		/* If region highlighting is on */

	if (cursor_start)
		i = CI(b);
	else
		i = b->d;

	*cp_set = 0;

	hl_on = 0;
	if (b->m_set && b->m < i) {
		attron(A_STANDOUT);
		hl_on = 1;
	}

	while (i <= EI(b)) {
		/* Highlighting */
		if (b->m_set && i == b->m) {
			if (hl_on) {
				attroff(A_STANDOUT);
				hl_on = 0;
			} else {
				attron(A_STANDOUT);
				hl_on = 1;
			}
		}
		if (i == CI(b)) {
			/* Record cursor screen position */
			getyx(stdscr, *cy, *cx);
			*cp_set = 1;

			if (hl_on) {
				attroff(A_STANDOUT);
				hl_on = 0;
			}
			if (b->m_set && b->m > i) {
				attron(A_STANDOUT);
				hl_on = 1;
			}
		}

		/* Print character */
		ch = READ(b, i);
		if (isprint(ch) || ch == '\t' || ch == '\n') {
			/* If printing is off screen */
			if (addch(ch) == ERR) {
				return;
			}
		} else {
			/*
			 * Unprintable character.
			 * If printing is off screen.
			 */
			if (addch('?') == ERR) {
				return;
			}
		}
		++i;
	}
}

int drawscreen(struct ed *e)
{
	struct buf *b = e->t[e->ab];	/* Active text buffer pointer */
	struct buf *cl = e->cl;	/* Command line buffer pointer */
	int h;			/* Height of screen */
	int w;			/* Width of screen */
	int th;			/* Height of text portion of the screen */
	int cp_set;		/* If the cursor position has been set */
	int cy;			/* Text buffer cursor y index */
	int cx;			/* Text buffer cursor x index */
	int cl_cy;		/* Command line cursor y index */
	int cl_cx;		/* Command line cursor x index */
	char *sb = NULL;	/* Status bar */
	size_t sb_s;		/* Size of the status bar */

	getmaxyx(stdscr, h, w);

	/* Cannot draw on a screen this small */
	if (h < 3 || w < 1) {
		LOG("screen too small");
		return -1;
	}

	/* Height of text portion of the screen */
	th = h - 2;

	if (th && w > INT_MAX / th) {
		LOG("integer overflow");
		return -1;
	}

	/* If need to centre */
	/*      if (b->v || b->r < b->t || b->r >= b->t + th || CI(b) < b->d || b->d - CI(b) > (size_t)(th * w))
	   centre(b, th);
	 */

	/* 1st attempt: from t line */
	if (erase() == ERR) {
		LOG("erase failed");
		return -1;
	}
	cy = 0;
	cx = 0;
	drawbuf(b, &cp_set, &cy, &cx, 0);

	/* 2nd attempt: draw from start of cursor's line */
	if (!cp_set || cy >= th) {
		if (erase() == ERR) {
			LOG("erase failed");
			return -1;
		}
		b->t = b->r;
		cy = 0;
		cx = 0;
		drawbuf(b, &cp_set, &cy, &cx, 0);

		/* 3rd attempt: draw from the cursor */
		if (!cp_set || cy >= th) {
			if (erase() == ERR) {
				LOG("erase failed");
				return -1;
			}
			cy = 0;
			cx = 0;
			drawbuf(b, &cp_set, &cy, &cx, 1);
		}
	}

	/* Status bar */
	if (safeadd(&sb_s, 2, (size_t) w, 1)) {
		LOG("safeadd failed");
		return -1;
	}

	if ((sb = malloc(sb_s)) == NULL) {
		LOG("malloc failed");
		return -1;
	}

	/* Create status bar */
	if (snprintf(sb, sb_s,
		     "%lu%c%c:%s (%lu) %02X [g:%lu, c:%lu, s:%lu, CI:%lu, m:%lu, d:%lu]",
		     e->ab, b->mod ? '*' : ' ', e->in_ret == -1 ? 'F' : ' ',
		     b->fn, b->r, (unsigned char)*b->c, (size_t) (b->g - b->a),
		     (size_t) (b->c - b->a), b->s, CI(b), b->m, b->d) < 0) {
		LOG("snprintf failed");
		free(sb);
		sb = NULL;
		return -1;
	}

	/* Clear status bar line */
	if (move(h - 2, 0) == ERR) {
		LOG("move failed");
		return -1;
	}
	if (clrtoeol() == ERR) {
		LOG("clrtoeol failed");
		return -1;
	}

	/* Add status bar */
	if (addnstr(sb, w) == ERR) {
		LOG("addnstr failed");
		free(sb);
		sb = NULL;
		return -1;
	}

	free(sb);
	sb = NULL;

	/* Highlight status bar */
	if (mvchgat(h - 2, 0, w, A_STANDOUT, 0, NULL) == ERR) {
		LOG("mvchgat failed");
		return -1;
	}

	/* First draw command line attempt */
	if (move(h - 1, 0) == ERR) {
		LOG("move failed");
		return -1;
	}
	if (clrtoeol() == ERR) {
		LOG("clrtoeol failed");
		return -1;
	}
	cl_cy = h - 1;
	cl_cx = 0;
	drawbuf(cl, &cp_set, &cl_cy, &cl_cx, 0);

	/* Second draw command line */
	if (!cp_set || cl_cx >= w) {
		if (move(h - 1, 0) == ERR) {
			LOG("move failed");
			return -1;
		}
		if (clrtoeol() == ERR) {
			LOG("clrtoeol failed");
			return -1;
		}
		cl_cy = h - 1;
		cl_cx = 0;
		drawbuf(cl, &cp_set, &cl_cy, &cl_cx, 1);
	}

	/* Set cursor */
	if (e->cl_active) {
		if (move(cl_cy, cl_cx) == ERR) {
			LOG("move failed");
			return -1;
		}
	} else {
		if (move(cy, cx) == ERR) {
			LOG("move failed");
			return -1;
		}
	}

	if (refresh() == ERR) {
		LOG("refresh failed");
		return -1;
	}

	return 0;
}

struct ed *inited(void)
{
	struct ed *e = NULL;

	if ((e = calloc(1, sizeof(struct ed))) == NULL) {
		LOG("calloc failed");
		return NULL;
	}

	if ((e->cl = initbuf(0)) == NULL) {
		free(e);
		e = NULL;
		return NULL;
	}

	e->operation = -1;
	e->running = 1;

	return e;
}

void freeed(struct ed *e)
{
	size_t i;

	if (e != NULL) {
		if (e->t != NULL) {
			for (i = 0; i < e->s; ++i) {
				freebuf(e->t[i]);
			}
			free(e->t);
			e->t = NULL;
		}

		free(e->k);
		e->k = NULL;

		freebuf(e->cl);

		free(e->cl_str);
		e->cl_str = NULL;

		free(e->search_str);
		e->search_str = NULL;

		free(e);
		e = NULL;
	}
}

int newbuf(struct ed *e, char *filename)
{
	size_t fs = 0;
	int load_file = 0;
	struct buf *new_buf = NULL;
	struct buf **new_t = NULL;
	size_t new_s;

	if (filename != NULL && strlen(filename)) {
		if (!access(filename, F_OK)) {
			if (filesize(&fs, filename)) {
				LOG("filesize failed");
				return -1;
			}
			if (fs)
				load_file = 1;
		}
	}

	if ((new_buf = initbuf(fs)) == NULL) {
		LOG("initbuf failed");
		return -1;
	}

	if (setfilename(new_buf, filename)) {
		LOG("setfilename failed");
		freebuf(new_buf);
		return -1;
	}

	if (load_file) {
		if (insertfile(new_buf, filename)) {
			LOG("insertfile failed");
			freebuf(new_buf);
			return -1;
		}

		/* Existing file read so clear modification indicator */
		new_buf->mod = 0;
	}

	if (safeadd(&new_s, 2, e->s, 1)) {
		LOG("safeadd failed");
		freebuf(new_buf);
		return -1;
	}

	if (MULTOF(new_s, sizeof(struct buf *))) {
		LOG("integer overflow");
		freebuf(new_buf);
		return -1;
	}

	if ((new_t = realloc(e->t, new_s * sizeof(struct buf *))) == NULL) {
		freebuf(new_buf);
		return -1;
	}

	e->t = new_t;
	e->s = new_s;

	/* Set active buffer to new buffer */
	e->ab = e->s - 1;
	e->t[e->ab] = new_buf;

	return 0;
}

void nextbuf(struct ed *e)
{
	if (e->ab == e->s - 1) {
		e->ab = 0;
	} else {
		++e->ab;
	}
}

void previousbuf(struct ed *e)
{
	if (!e->ab) {
		e->ab = e->s - 1;
	} else {
		--e->ab;
	}
}

void keycx(struct ed *e)
{
	struct buf *b = NULL;
	int x;

	if (e->cl_active)
		b = e->cl;
	else
		b = e->t[e->ab];

	x = getch();
	switch (x) {
	case KEY_LEFT:
		previousbuf(e);
		break;
	case KEY_RIGHT:
		nextbuf(e);
		break;
	case Cc:
		e->running = 0;
		break;
	case Cf:
		e->cl_active = 1;
		e->operation = Cf;
		break;
	case Cg:
		break;
	case Cs:
		e->in_ret = save(b);
		break;
	case Cr:
		e->cl_active = 1;
		e->operation = Cr;
		break;
	case 'i':
		e->cl_active = 1;
		e->operation = 'i';
		break;
	}
}

void keyesc(struct ed *e)
{
	struct buf *b = NULL;
	int x;

	if (e->cl_active)
		b = e->cl;
	else
		b = e->t[e->ab];

	x = getch();
	switch (x) {
	case Cg:
		break;
	case '<':
		first(b);
		break;
	case '>':
		last(b);
		break;
	case 'm':
		e->in_ret = matchbrace(b);
		break;
	case 'n':
		e->in_ret = search(b, e->search_str);
		break;
	case 't':
		trimwhitespace(b);
		break;
	case 'v':
		pageup(b);
		break;
	case 'w':
		e->in_ret = killregion(b, &e->k, &e->ks, &e->kn, 0);
		break;
	case 'x':
		e->cl_active = 1;
		e->operation = 'x';
		break;
	}
}

void keyn(struct ed *e)
{
	struct buf *b = NULL;
	char **dst = NULL;

	if (e->operation == Cs)
		dst = &e->search_str;
	else
		dst = &e->cl_str;

	if (buftostr(e->cl, dst)) {
		/* Clear operation */
		e->cl_active = 0;
		e->operation = -1;
		return;
	}

	/* Clear the command line buffer */
	deletebuf(e->cl);

	/* Active buffer */
	b = e->t[e->ab];

	switch (e->operation) {
	case Cf:
		e->in_ret = newbuf(e, e->cl_str);
		break;
	case Cr:
		e->in_ret = setfilename(b, e->cl_str);
		break;
	case Cs:
		e->in_ret = search(b, e->search_str);
		break;
	case 'i':
		e->in_ret = insertfile(b, e->cl_str);
		break;
	}

	e->cl_active = 0;
	e->operation = -1;
}

void key(struct ed *e)
{
	struct buf *b = NULL;
	int x;

	if (e->cl_active)
		b = e->cl;
	else
		b = e->t[e->ab];

	x = getch();

	switch (x) {
	case Cspc:
		setmark(b);
		break;
	case Ca:
	case KEY_HOME:
		home(b);
		break;
	case Cb:
	case KEY_LEFT:
		e->in_ret = leftch(b);
		break;
	case Cd:
	case KEY_DC:
		e->in_ret = deletech(b);
		break;
	case Ce:
	case KEY_END:
		end(b);
		break;
	case Cf:
	case KEY_RIGHT:
		e->in_ret = rightch(b);
		break;
	case Cg:
		if (b->m_set) {
			b->m_set = 0;
		} else if (e->cl_active) {
			e->cl_active = 0;
			e->operation = -1;
		}
		break;
	case Ch:
	case Cqm:
	case KEY_BACKSPACE:
		e->in_ret = backspacech(b);
		break;
	case Ck:
		e->in_ret = killfwdline(b, &e->k, &e->ks, &e->kn);
		break;
	case Cl:
		level(b);
		break;
	case Cn:
	case KEY_DOWN:
		e->in_ret = down(b);
		break;
	case Cp:
	case KEY_UP:
		e->in_ret = up(b);
		break;
	case Cq:
		e->in_ret = inserthex(b);
		break;
	case Cs:
		e->cl_active = 1;
		e->operation = Cs;
		break;
	case Cu:
		e->in_ret = uproot(b, &e->k, &e->ks, &e->kn);
		break;
	case Cv:
	case KEY_NPAGE:
		pagedown(b);
		break;
	case KEY_PPAGE:
		pageup(b);
		break;
	case Cw:
		e->in_ret = killregion(b, &e->k, &e->ks, &e->kn, 1);
		break;
	case Cx:
		keycx(e);
		break;
	case Cy:
		e->in_ret = yank(b, e->k, e->ks, e->kn);
		break;
	case ESC:
		keyesc(e);
		break;
	case '\n':
		if (e->cl_active) {
			keyn(e);
		} else {
			e->in_ret = insertch(b, x);
		}
		break;
	default:
		if (isprint(x) || x == '\t')
			e->in_ret = insertch(b, x);
	}
}

int main(int argc, char **argv)
{
	struct ed *e = NULL;
	int i;

	if ((e = inited()) == NULL) {
		LOG("inited failed");
		return 1;
	}

	if (argc < 1) {
		LOG("argc is less than one");
		return 1;
	}

	if (argc == 1) {
		if (newbuf(e, NULL)) {
			LOG("newbuf failed");
			freeed(e);
			return 1;
		}
	} else {
		for (i = 1; i < argc; ++i) {
			if (newbuf(e, argv[i])) {
				LOG("newbuf failed");
				freeed(e);
				return 1;
			}
		}

		/* Start with first file specified */
		e->ab = 0;
	}

	if (initnc()) {
		LOG("initnc failed");
		freeed(e);
		return 1;
	}

	while (e->running) {
		if (drawscreen(e))
			LOG("drawscreen failed");

		e->in_ret = 0;

		key(e);
	}

	if (freenc()) {
		LOG("freenc failed");
		freeed(e);
		return 1;
	}

	freeed(e);
	return 0;
}
