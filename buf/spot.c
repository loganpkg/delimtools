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

#define LOG(m) fprintf(stderr, "%s:%d: error: " m "\n", __FILE__, __LINE__)
/* size_t addtion overflow test */
#define ADDOF(a, b) ((a) > SIZE_MAX - (b))
/* size_t multiplication overflow test */
#define MULTOF(a, b) ((a) && (b) > SIZE_MAX / (a))

#define INSERT(b, ch) do { \
if (ch == '\n') ++b->r; \
*b->g = ch; \
++b->g; \
} while (0)

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
	char ch;

	if (b->c == b->a + b->s - 1)
		return -1;

	b->m_set = 0;
	b->mod = 1;
	ch = *b->c;
	++b->c;
	return ch;
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

void end(struct buf *b) {
  int x;
  while ((x = rightch(b)) != -1) {
    if (x == '\n') break;
  }
}

void home(struct buf *b) {
  int x;
  while ((x = leftch(b)) != -1) {
    if (x == '\n') {
      rightch(b);
      break;
    }
  }
}

void first(struct buf *b) {
  while(leftch(b) != -1);
}

void last(struct buf *b) {
  while(right(b) != -1);
}

int hexnum(int *h, int c)
{
	if (!isxdigit(c)) return -1;

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
	if (hex_num(&h0, c0)) return;

	c1 = getch();
	if (hex_num(&h1, c1)) return;

	insertch(b, h0 * 16 + h1);
}
