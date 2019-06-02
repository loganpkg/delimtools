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

/* Default gap size */
#define GAP 100

#define REGION_COLORS 1

#define LOG(m) fprintf(stderr, "%s:%d: error: " m "\n", __FILE__, __LINE__)
/* size_t addtion overflow test */
#define ADDOF(a, b) ((a) > SIZE_MAX - (b))
/* size_t multiplication overflow test */
#define MULTOF(a, b) ((a) && (b) > SIZE_MAX / (a))

#define INSERT(b, ch) do { \
    if ((ch) == '\n') ++(b)->r; \
    *((b)->g++) = (ch);		\
} while (0)

#define INSERTANDLEFT(b, ch) (*(--(b)->c) = (ch))

#define DELETE(b) (*((b)->c++))
#define BACKSPACE(b) (*(--(b)->g))

struct buf {
	char *fn;		/* Filename */
	char *a;		/* Array */
	char *g;		/* Start of gap */
	char *c;		/* Cursor */
	char *m;		/* Mark */
	size_t s;		/* Size of array */
	size_t r;		/* Cursor's row number */
	size_t t;		/* Row number of top of screen */
	int m_set;		/* If the mark is set */
	int mod;		/* If the buffer is modified */
	int v;			/* veritcal centring requested */
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
			va_end(ap);
			return -1;
		}
		total += arg_val;
	}
	va_end(ap);
	*res = total;
	return 0;
}

int growgap(struct buf *b, size_t will_use)
{
	char *new_a = NULL;
	size_t new_s;
	size_t g_index = b->g - b->a;
	size_t c_index = b->c - b->a;

	if (will_use <= b->c - b->g)
		return 0;

	if (safeadd(&new_s, 3, b->s, will_use, GAP)) {
		LOG("safeadd failed");
		return -1;
	}
	new_s -= b->c - b->g;

	if ((new_a = realloc(b->a, new_s)) == NULL) {
		LOG("realloc failed");
		return -1;
	}

	memmove(new_a + c_index + new_s - b->s, new_a + c_index,
		b->s - c_index);

	b->a = new_a;
	b->g = new_a + g_index;
	b->c = new_a + c_index;
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

	INSERT(b, ch);
	return 0;
}

int deletech(struct buf *b)
{
	if (b->c == b->a + b->s - 1)
		return -1;

	b->m_set = 0;
	b->mod = 1;

	return DELETE(b);
}

int backspace(struct buf *b)
{
	if (b->g == b->a)
		return -1;

	b->m_set = 0;
	b->mod = 1;

	return BACKSPACE(b);
}

int leftch(struct buf *b)
{
	if (b->g == b->a)
		return -1;

	--b->g;
	--b->c;
	*b->c = *b->g;

	if (b->m_set && b->g == b->m)
		b->m = b->c;

	if (*b->c == '\n')
		--b->r;

	return *b->c;
}

int rightch(struct buf *b)
{
	if (b->c == b->a + b->s - 1)
		return -1;

	if (b->m_set && b->c == b->m)
		b->m = b->g;

	if (*b->c == '\n')
		++b->r;

	*b->g = *b->c;
	++b->g;
	++b->c;
	return *b->c;
}

int insertbuf(struct buf *b, struct buf *k)
{
	int x;

	if (growgap(b, k->s - (k->c - k->g))) {
		LOG("growgap failed");
		return -1;
	}

	b->m_set = 0;
	b->mod = 1;

	first(k);
	INSERT(b, *k->c);
	while ((x = right(k)) != -1)
		INSERT(b, x);

	return 0;
}

struct buf *initbuf(void) {
  struct buf *b = NULL;

  if ((b = calloc(1, sizeof(struct buf))) == NULL) {
    LOG("calloc failed");
    return NULL;
  }

  return b;
}

void freebuf(struct buf *b) {
  if (b != NULL) {
    free(b->fn);
    b->fn = NULL;
    free(b->a);
    b->a = NULL;
    free(b);
    b = NULL;
  }
}

void setmark(struct buf *b)
{
	b->m_set = 1;
	b->m = b->c;
}

int killregion(struct buf *b, struct buf *k)
{

	if (!b->m_set || b->c == b->m)
		return -1;

	/* Cursor before mark */
	if (b->c < b->m) {
		if (growgap(b, b->m - b->c)) {
			LOG("growgap failed");
			return -1;
		}

		b->m_set = 0;
		b->mod = 1;

		k->m_set = 0;
		k->mod = 1;

		while (b->c != b->m)
			INSERT(k, DELETE(b));
	} else {
		/* Mark before cursor */
		if (growgap(b, b->g - b->m)) {
			LOG("growgap failed");
			return -1;
		}

		b->m_set = 0;
		b->mod = 1;

		k->m_set = 0;
		k->mod = 1;

		while (b->g != b->m)
			INSERTANDLEFT(k, BACKSPACE(b));
	}
	return 0;
}

int filesize(size_t * fs, char *fn)
{
	struct stat st;

	if (fn == NULL) {
		LOG("NULL filename");
		return -1;
	}

	if (stat(fn, &st)) {
		LOG("stat failed");
		return -1;
	}

	if (!S_ISREG(st.st_mode)) {
		LOG("Not regular file");
		return -1;
	}

	if (st.st_size < 0) {
		LOG("Negative file size");
		return -1;
	}

	*fs = (size_t) st.st_size;
	return 0;
}

int insertfile(stuct buf * b, char *fn)
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
		b->mod = 1;
	}

	return ret;
}

int save(struct buf *b)
{
	int ret = 0;
	FILE *fp = NULL;

	if (!b->mod)
		return 0;

	if ((fp = fopen(b->fn, "w")) == NULL) {
		LOG("fopen failed");
		return -1;
	}

	/* Before gap */
	if (fwrite(b->a, 1, b->g - b->a, fp) != b->g - b->a) {
		LOG("fwrite failed before gap");
		ret = -1;
		goto clean_up;
	}

	/* After gap */
	if (fwrite(b->c, 1, b->s - 1, fp) != b->s - 1) {
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

void end(struct buf *b)
{
	int x;
	while ((x = rightch(b)) != -1) {
		if (x == '\n')
			break;
	}
}

void home(struct buf *b)
{
	int x;
	while ((x = leftch(b)) != -1) {
		if (x == '\n') {
			rightch(b);
			break;
		}
	}
}

void first(struct buf *b)
{
	while (leftch(b) != -1) ;
}

void last(struct buf *b)
{
	while (right(b) != -1) ;
}

int matchbrace(struct buf *b)
{
	int (*move) (struct buf *);
	char original;
	char target;
	int fwd;
	char ch;
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

	if (fwd) {
		move = &rightch;
	} else {
		move = &leftch;
	}

	depth = 0;
	found = 0;
	while (!(*move) (b)) {
		ch = *b->c;
		if (ch == original) {
			++depth;
		}
		if (ch == target) {
			if (depth == 0) {
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
	char ch;
	int line_feed;
	int end_of_line;

	last(b);

	/* Trim end of buffer */
	line_feed = 0;
	while (leftch(b) != -1) {
		ch = *b->c;
		if (ch == '\n') {
			if (!line_feed) {
				line_feed = 1;
			} else {
				deletech(b);
			}
		} else if (ch == ' ' || ch == '\t') {
			deletech(b);
		} else {
			break;
		}
	}

	/* Trim body */
	end_of_line = 1;
	while (leftch(b) != -1) {
		ch = *b->c;
		switch (ch) {
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

int hexnum(int *h, int c)
{
	if (!isxdigit(c))
		return -1;

	if (isdigit(c))
		*h = c - '0';
	else if (islower(c))
		*h = c - 'a' + 10;
	else
		*h = c - 'A' + 10;

	return 0;
}

void inserthex(struct buf *b)
{
	int c0, c1, h0, h1;

	c0 = getch();
	if (hex_num(&h0, c0))
		return;

	c1 = getch();
	if (hex_num(&h1, c1))
		return;

	insertch(b, h0 * 16 + h1);
}

int insertshell(struct buf *b, char *shellcmd, int *shellret)
{
	int ret = 0;
	FILE *fp = NULL;
	int x;
	int st;

	if ((fp = popen(shellcmd, "r")) == NULL) {
		LOG("popen failed");
		return -1;
	}

	while ((x = getc(fp)) != EOF) {
		if (insertch(b, x)) {
			LOG("insertch failed");
			ret = -1;
			goto clean_up;
		}
	}

 clean_up:
	if ((st = pclose(fp)) == -1) {
		LOG("pclose failed");
		ret = -1;
	} else {
		if (WIFSIGNALED(st)) {
			*shellret = WTERMSIG(st);
		} else if (WIFEXITED(st)) {
			*shellret = WEXITSTATUS(st);
		}
	}

	return ret;
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

	if (has_colors() == FALSE) {
		LOG("has_colors failed");
		ret = -1;
		goto clean_up;
	}

	if (start_color() == ERR) {
		LOG("start_color failed");
		ret = -1;
		goto clean_up;
	}

	if (init_pair(REGION_COLORS, COLOR_YELLOW, COLOR_MAGENTA) == ERR) {
		LOG("init_pair failed");
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

	b->v = 0;
}

void drawbuf(struct buf *b, int *cp_set, int *cy, int *cx, int cursor_start)
{
	char *p, end_of_buf;
	size_t count;
	int hl_on;		/* If region highlighting is on */

	if (cursor_start) {
		p = b->c;
	} else {
		/* Search backwards to find the start of row t */
		if (!b->t) {
			p = b->a;
		} else {
			p = b->g - 1;
			count = 0;

			while (p != b->a) {
				if (*p == '\n') {
					++count;
				}

				if (count == b->r - b->t + 1) {
					break;
				}
				--p;
			}

			if (*p == '\n') {
				++p;
			}
		}
	}

	*cp_set = 0;

	hl_on = 0;
	if (b->m_set && b->m < p) {
		attron(COLOR_PAIR(REGION_COLORS));
		hl_on = 1;
	}

	end_of_buf = b->a + b->s;
	while (p < end_of_buf) {
		/* Gap */
		if (p == b->g) {
			/* Skip gap */
			p = b->c;

			/* Record cursor position */
			getyx(stdscr, *cy, *cx);
			*cp_set = 1;

			if (hl_on) {
				attroff(COLOR_PAIR(REGION_COLORS));
				hl_on = 0;
			}
			if (b->m_set && b->m > p) {
				attron(COLOR_PAIR(REGION_COLORS));
				hl_on = 1;
			}
		} else {
			if (b->m_set && p == b->m) {
				if (hl_on) {
					attroff(COLOR_PAIR(REGION_COLORS));
					hl_on = 0;
				} else {
					attron(COLOR_PAIR(REGION_COLORS));
					hl_on = 1;
				}
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
	/* Height and width of screen */
	int h;
	int w;
	int th;			/* Height of text portion of the screen */
	int cp_set;		/* If the cursor position has been set */
	/* Text buffer cursor index */
	int cy;
	int cx;
	/* Command line cursor index */
	int cl_cy;
	int cl_cx;
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

	/* If need to centre */
	if (b->r < b->t || b->r >= b->t + th)
		centre(b, th);

	/* 1st attempt: from t line */
	if (erase() == ERR) {
		LOG("erase failed");
		return -1;
	}
	cy = 0;
	cx = 0;
	bdraw(b, &cp_set, &cy, &cx, 0);

	/* 2nd attempt: draw from start of cursor's line */
	if (!cp_set || cy >= th) {
		if (erase() == ERR) {
			LOG("erase failed");
			return -1;
		}
		b->t = b->r;
		cy = 0;
		cx = 0;
		bdraw(b, &cp_set, &cy, &cx, 0);

		/* 3rd attempt: draw from the cursor */
		if (!cp_set || cy >= th) {
			if (erase() == ERR) {
				LOG("erase failed");
				return -1;
			}
			cy = 0;
			cx = 0;
			bdraw(b, &cp_set, &cy, &cx, 1);
		}
	}

	/* Status bar */
	if (ADDOF((size_t) w, 1)) {
		LOG("addition integer overflow");
		return -1;
	}

	sb_s = w + 1;

	if ((sb = malloc(sb_s)) == NULL) {
		LOG("malloc failed");
		return -1;
	}

	/* Create status bar */
	if (snprintf(sb, sb_s, "%c%c %s (%lu) %02x %d %s",
		     (b->mod) ? '*' : ' ',
		     (e->cmd_ret) ? 'F' : ' ',
		     b->fn,
		     b->r, (unsigned char)b->c, e->shell_ret, e->msg) < 0) {
		LOG("snprintf failed");
		free(sb);
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
		return -1;
	}

	free(sb);

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
	bdraw(cl, &cp_set, &cl_cy, &cl_cx, 0);

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
		bdraw(cl, &cp_set, &cl_cy, &cl_cx, 1);
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

