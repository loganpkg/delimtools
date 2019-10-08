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

#include <buf.h>
#include <gen.h>
#include <ctype.h>
#include <curses.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Editor */
struct ed {
	buf **t;		/* Array of text buffers */
	size_t s;		/* Size of text buffer array */
	size_t ab;		/* Active text buffer */
	char *k;		/* Kill array */
	size_t ks;		/* Size of kill array */
	size_t kn;		/* Number of \n characters in kill array */
	buf *cl;		/* Command line buffer */
	char *cl_str;		/* Command line string */
	char *search_str;	/* Search string */
	int cl_active;		/* Editing in command line */
	int operation;		/* Operation that requires command line */
	int in_ret;		/* Return value of internal operation */
	int running;		/* Editor is running */
};

int inserthex(buf *b)
{
	int x0, x1, h0, h1;

	x0 = getch();
	if (hextonum(x0, &h0))
		return -1;

	x1 = getch();
	if (hextonum(x1, &h1))
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

void level(buf *b)
{
	if (clear() == ERR)
		LOG("clear failed");

	VCR(b) = 1;
}

void centre(buf *b, int th)
{
	/*
	 * This function sets the top of screen row number, TSRN(b),
	 * so that the cursor will be in the middle of the text portion
	 * of the screen (assuming there are no long lines creating
	 * line wrapping).
	 * It then does a reverse scan to set the draw start index, DSI(b),
	 * to the start of the TSRN(b) line.
	 */

	size_t row;

	/*
	 * Max row index where centring will result in TSRN(b)
	 * being set to zero.
	 */
	size_t t_zero_range;

	/* If text portion screen height is odd */
	if (th % 2) {
		t_zero_range = th / 2;
	} else {
		t_zero_range = th / 2 - 1;
	}

	/* If near top of buffer */
	if (CRN(b) <= t_zero_range) {
	  TSRN(b) = 0;
	} else {
	  TSRN(b) = CRN(b) - t_zero_range;
	}

	if (!TSRN(b) || !CI(b)) {
	  DSI(b) = 0;
	  VCR(b) = 0;
		return;
	}

	/*
	 * Set draw start index by
	 * searching backwards to find the start of the TSRN(b) line.
	 * CI was checked for zero above.
	 */
	DSI(b) = CI(b) - 1;
	row = CRN(b);
	while (DSI(b)) {
	  if (READ(b, DSI(b)) == '\n') {
			--row;
			if (row == TSRN(b) - 1) {
			  ++DSI(b);
				break;
			}
		}

	  --DSI(b);
	}

	VCR(b) = 0;
}

void drawbuf(buf *b, int *cp_set, int *cy, int *cx)
{
	size_t i;
	char ch;
	int hl_on;		/* If region highlighting is on */

	/*
	 * The graphics do not rely on the gap buffer structure,
	 * so that the buffer method could be changed without
	 * rewriting the graphics.
	 */

	i = DSI(b);

	/* Cursor position not set */
	*cp_set = 0;

	hl_on = 0;
	if (MSET(b) && MI(b) < i) {
		attron(A_STANDOUT);
		hl_on = 1;
	}

	while (i <= EI(b)) {
		/* Highlighting */
	  if (MSET(b) && i == MI(b)) {
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
			if (MSET(b) && MI(b) > i) {
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
	buf *b = e->t[e->ab];	/* Active text buffer pointer */
	buf *cl = e->cl;	/* Command line buffer pointer */
	int h;			/* Height of screen */
	int w;			/* Width of screen */
	int th;			/* Height of text portion of the screen */
	int cp_set;		/* If the cursor position has been set */
	int cy;			/* Text buffer cursor y position on screen */
	int cx;			/* Text buffer cursor x position on screen */
	int cl_cy;		/* Command line cursor y position on screen */
	int cl_cx;		/* Command line cursor x position on screen */
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

	/*
	 * Test if need to centre.
	 * The draw start index will eventually be set so that the
	 * cursor is on the screen before editing resumes.
	 * Hence, draw start is always less than or equal to the cursor
	 * index. This is why draw start does not need to be updated by
	 * inserts and deletes, as they cannot happen before it.
	 */
	if (VCR(b) ||
	    CRN(b) < TSRN(b) ||
	    CRN(b) >= TSRN(b) + th ||
	    CI(b) < DSI(b) || CI(b) - DSI(b) > (size_t) (th * w))
		centre(b, th);


	/* Cleanup graphics */
	if (erase() == ERR) {
		LOG("erase failed");
		return -1;
	}
	cy = 0;
	cx = 0;

	/*
	 * 1st attempt:
	 * This could fail if there are long lines pushing
	 * the cursor off the text portion of the screen.
	 */
	drawbuf(b, &cp_set, &cy, &cx);

	/* If cursor is off text portion of screen */
	if (!cp_set || cy >= th) {
		/* Cleanup graphics */
		if (erase() == ERR) {
			LOG("erase failed");
			return -1;
		}
		cy = 0;
		cx = 0;

		/* Set draw start index to cursor index */
		DSI(b) = CI(b);
		/* Set top of screen row number to cursor row number */
		TSRN(b) = CRN(b);
		/*
		 * 2nd attempt:
		 * This will succeed as it is commencing from the cursor.
		 */
		drawbuf(b, &cp_set, &cy, &cx);
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
		     "%lu%c%c:%s (%lu) %02X",
		     e->ab,
		     BMOD(b) ? '*' : ' ',
		     e->in_ret == -1 ? 'F' : ' ',
		     FN(b),
		     CRN(b),
		     (unsigned char)CCH(b)) < 0) {
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

	/* Clear command line */
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

	/*
	 * 1st attempt:
	 * This could fail if the cursor is too far along a line,
	 * pushing the cursor off the screen.
	 */
	drawbuf(cl, &cp_set, &cl_cy, &cl_cx);

	/* If cursor is off the screen */
	if (!cp_set || cl_cx >= w) {
		/* Clear command line */
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

		/* Set draw start index to cursor index */
		DSI(cl) = CI(cl);
		/* Set top of screen row number to cursor row number */
		TSRN(cl) = CRN(cl);
		/*
		 * 2nd attempt:
		 * This will succeed as it is commencing from the cursor.
		 */
		drawbuf(cl, &cp_set, &cl_cy, &cl_cx);
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
	buf *new_buf = NULL;
	buf **new_t = NULL;
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

	if (MULTOF(new_s, sizeof(buf *))) {
		LOG("integer overflow");
		freebuf(new_buf);
		return -1;
	}

	if ((new_t = realloc(e->t, new_s * sizeof(buf *))) == NULL) {
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
	if (e->ab == e->s - 1)
		e->ab = 0;
	else
		++e->ab;
}

void previousbuf(struct ed *e)
{
	if (!e->ab)
		e->ab = e->s - 1;
	else
		--e->ab;
}

void keycx(struct ed *e)
{
	buf *b = NULL;
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
	buf *b = NULL;
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
	case '{':
		backwardpara(b);
		break;
	case '}':
		forwardpara(b);
		break;
	case 'a':
		backwardsent(b);
		break;
	case 'b':
		backwardword(b);
		break;
	case 'e':
		forwardsent(b);
		break;
	case 'f':
		forwardword(b);
		break;
	case 'l':
		lowercaseword(b);
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
	case 'u':
		uppercaseword(b);
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
	buf *b = NULL;
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
	buf *b = NULL;
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
		homeofline(b);
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
		endofline(b);
		break;
	case Cf:
	case KEY_RIGHT:
		e->in_ret = rightch(b);
		break;
	case Cg:
	  if (MSET(b)) {
	    MSET(b) = 0;
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
		if (e->cl_active)
			keyn(e);
		else
			e->in_ret = insertch(b, x);
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
