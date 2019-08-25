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
#include <buf.h>
#include <curses.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

	if (!b->t || b->g == b->a) {
		b->d = b->a;
		b->v = 0;
		return;
	}

	/* Search backwards to find the start of row t */
	b->d = b->g - 1;
	row = b->r;
	while (b->d != b->a) {
		if (*b->d == '\n') {
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
	char *p, *end_of_buf;
	int hl_on;		/* If region highlighting is on */

	if (cursor_start)
		p = b->g;
	else
		p = b->d;

	*cp_set = 0;

	hl_on = 0;
	if (b->m_set && b->m < p) {
		attron(A_STANDOUT);
		hl_on = 1;
	}

	/* Before the gap */
	while (p < b->g) {
		if (b->m_set && p == b->m) {
			if (hl_on) {
				attroff(A_STANDOUT);
				hl_on = 0;
			} else {
				attron(A_STANDOUT);
				hl_on = 1;
			}
		}

		if (isprint(*p) || *p == '\t' || *p == '\n') {
			/* If printing is off screen */
			if (addch(*p) == ERR) {
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
		++p;
	}

	/* In the gap: record cursor screen position */
	getyx(stdscr, *cy, *cx);
	*cp_set = 1;

	if (hl_on) {
		attroff(A_STANDOUT);
		hl_on = 0;
	}
	if (b->m_set && b->m > p) {
		attron(A_STANDOUT);
		hl_on = 1;
	}

	/* After the gap */
	p = b->c;
	end_of_buf = b->a + b->s - 1;
	while (p <= end_of_buf) {
		if (b->m_set && p == b->m) {
			if (hl_on) {
				attroff(A_STANDOUT);
				hl_on = 0;
			} else {
				attron(A_STANDOUT);
				hl_on = 1;
			}
		}

		if (isprint(*p) || *p == '\t' || *p == '\n') {
			/* If printing is off screen */
			if (addch(*p) == ERR) {
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
		++p;
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
	if (b->v || b->r < b->t || b->r >= b->t + th || b->g - b->d > th * w)
		centre(b, th);

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
	if (snprintf
	    (sb, sb_s,
	     "%lu%c%c:%s (%lu) %02X "
	     "[t:%lu di:%lu gi:%lu ci:%lu s:%lu m_set:%d mi:%lu]",
	     e->ab, b->mod ? '*' : ' ', e->in_ret == -1 ? 'F' : ' ', b->fn,
	     b->r, (unsigned char)*b->c,
	     b->t, b->d - b->a, b->g - b->a, b->c - b->a, b->s, b->m_set,
	     b->m_set ? b->m - b->a : 0) < 0) {
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
