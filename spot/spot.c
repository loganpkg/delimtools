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

/*
 * spot: text editor.
 * Dedicated to my son who was only a 4mm "spot" in his first ultrasound.
 *
 * $ cc -ansi -g -O3 -Wall -Wextra -pedantic spot.c -lncurses && mv a.out spot
 */

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <curses.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <ctype.h>

/* Initial buffer size */
#define INIT_BUF_SIZE BUFSIZ

/* size_t Addition OverFlow test */
#define AOF(a, b) ((a) > SIZE_MAX - (b))
/* size_t Multiplication OverFlow test */
#define MOF(a, b) ((a) && (b) > SIZE_MAX / (a))

/* Takes signed input */
#define ISASCII(x) ((x) >= 0 && (x) <= 127)

/* Converts a lowercase letter to it's control value */
#define CTRL(l) ((l) - 'a' + 1)

/* Control spacebar or control @ */
#define CTRL_SPC 0

/* Escape key */
#define ESC 27

/* Calculates the gap size */
#define GAPSIZE(b) ((size_t) (b->c - b->g))

/* Converts the cursor pointer to an index */
#define CURSOR_INDEX(b) ((size_t) (b->g - b->a))

/* Converts an index to a pointer */
#define INDEX_TO_POINTER(b, i) (b->a + b->i < b->g ? b->a + b->i \
				: b->c + b->i - (b->g - b->a))

/* Delete buffer */
#define DELETEBUF(b) do {b->g = b->a; b->c = b->e; b->r = 1; b->d = 0; \
             b->m = 0; b->mr = 1; b->m_set = 0; b->mod = 1;} while (0)

/* Update settings when a buffer is modified */
#define SETMOD(b) do {b->m = 0; b->mr = 1; b->m_set = 0; b->mod = 1;} while (0)

/* No bound or gap size checks are performed */
#define INSERTCH(b, x) do {*b->g++ = x; if (x == '\n') ++b->r;} while(0)
#define DELETECH(b) ++b->c
#define BACKSPACECH(b) if (*--b->g == '\n') --b->r
#define LEFTCH(b) do {*--b->c = *--b->g; if (*b->c == '\n') --b->r;} while(0)
#define RIGHTCH(b) do {if (*b->c == '\n') ++b->r; *b->g++ = *b->c++;} while (0)

/* gap buffer */
struct buf {
    struct buf *prev;           /* Previous buffer node */
    char *fn;                   /* Filename */
    char *a;                    /* Start of buffer */
    char *g;                    /* Start of gap */
    char *c;                    /* Cursor (to the right of the gap) */
    char *e;                    /* End of buffer */
    size_t r;                   /* Row number (starting from 1) */
    size_t sc;                  /* Sticky column number (starting from 0) */
    int sc_set;                 /* Sticky column is set */
    size_t d;                   /* Draw start index (ignores the gap) */
    size_t m;                   /* Mark index (ignores the gap) */
    size_t mr;                  /* Row number at the mark */
    int m_set;                  /* Mark is set */
    int mod;                    /* Buffer text has been modified */
    struct buf *next;           /* Next buffer node */
};

struct buf *init_buf(void)
{
    /* Initialises a buffer */
    struct buf *b;
    if ((b = malloc(sizeof(struct buf))) == NULL)
        return NULL;
    if ((b->a = malloc(INIT_BUF_SIZE)) == NULL)
        return NULL;
    b->e = b->a + INIT_BUF_SIZE - 1;
    b->g = b->a;
    b->c = b->e;
    /* End of buffer char. Cannot be deleted. */
    *b->e = '\0';
    b->r = 1;
    b->sc = 0;
    b->sc_set = 0;
    b->d = 0;
    b->m = 0;
    b->mr = 1;
    b->m_set = 0;
    b->mod = 0;
    b->fn = NULL;
    b->prev = NULL;
    b->next = NULL;
    return b;
}

void free_buf_list(struct buf *b)
{
    /* Frees a buffer doubly linked list (can be a single buffer) */
    struct buf *t = b, *next;
    if (b == NULL)
        return;
    /* Move to start of list */
    while (t->prev != NULL)
        t = t->prev;
    /* Move forward freeing each node */
    while (t != NULL) {
        next = t->next;
        free(t->fn);
        free(t->a);
        free(t);
        t = next;
    }
}

int grow_gap(struct buf *b, size_t will_use)
{
    /* Grows the gap size of a buffer */
    char *t, *new_e;
    size_t buf_size = b->e - b->a + 1;
    if (MOF(buf_size, 2))
        return 1;
    buf_size *= 2;
    if (AOF(buf_size, will_use))
        return 1;
    buf_size += will_use;
    if ((t = malloc(buf_size)) == NULL)
        return 1;
    /* Copy text before the gap */
    memcpy(t, b->a, b->g - b->a);
    /* Copy text after the gap, excluding the end of buffer character */
    new_e = t + buf_size - 1;
    memcpy(new_e - (b->e - b->c), b->c, b->e - b->c);
    /* Set end of buffer character */
    *new_e = '\0';
    /* Update pointers, indices do not need to be changed */
    b->g = t + (b->g - b->a);
    b->c = new_e - (b->e - b->c);
    b->e = new_e;
    /* Free old memory */
    free(b->a);
    b->a = t;
    return 0;
}

int insert_ch(struct buf *b, char c, size_t mult)
{
    /* Inserts a char mult times into the buffer */
    if (GAPSIZE(b) < mult)
        if (grow_gap(b, mult))
            return 1;
    while (mult--)
        INSERTCH(b, c);
    SETMOD(b);
    return 0;
}

int delete_ch(struct buf *b, size_t mult)
{
    /* Deletes mult chars in a buffer */
    if (mult > (size_t) (b->e - b->c))
        return 1;
    while (mult--)
        DELETECH(b);
    SETMOD(b);
    return 0;
}

int backspace_ch(struct buf *b, size_t mult)
{
    /* Backspaces mult chars in a buffer */
    if (mult > CURSOR_INDEX(b))
        return 1;
    while (mult--)
        BACKSPACECH(b);
    SETMOD(b);
    return 0;
}

int left_ch(struct buf *b, size_t mult)
{
    /* Move the cursor left mult positions */
    if (mult > CURSOR_INDEX(b))
        return 1;
    while (mult--)
        LEFTCH(b);
    return 0;
}

int right_ch(struct buf *b, size_t mult)
{
    /* Move the cursor right mult positions */
    if (mult > (size_t) (b->e - b->c))
        return 1;
    while (mult--)
        RIGHTCH(b);
    return 0;
}

void start_of_buf(struct buf *b)
{
    while (b->a != b->g)
        LEFTCH(b);
}

void end_of_buf(struct buf *b)
{
    while (b->c != b->e)
        RIGHTCH(b);
}

void start_of_line(struct buf *b)
{
    while (b->a != b->g && *(b->g - 1) != '\n')
        LEFTCH(b);
}

void end_of_line(struct buf *b)
{
    while (b->c != b->e && *b->c != '\n')
        RIGHTCH(b);
}

size_t col_num(struct buf *b)
{
    /* Returns the column number, which starts from zero */
    char *q = b->g;
    while (q != b->a && *(q - 1) != '\n')
        --q;
    return b->g - q;
}

int up_line(struct buf *b, size_t mult)
{
    /* Moves the cursor up mult lines */
    size_t col;
    size_t target_row;
    if (b->r > mult)
        target_row = b->r - mult;
    else
        return 1;

    /* Get or set sticky column */
    if (b->sc_set) {
        col = b->sc;
    } else {
        col = col_num(b);
        b->sc = col;
        b->sc_set = 1;
    }

    while (b->g != b->a && b->r != target_row)
        LEFTCH(b);
    start_of_line(b);
    while (b->c != b->e && col && *b->c != '\n') {
        RIGHTCH(b);
        --col;
    }
    return 0;
}

int down_line(struct buf *b, size_t mult)
{
    /* Moves the cursor down mult lines */
    size_t col;
    size_t target_row;
    char *c_backup = b->c;

    if (AOF(b->r, mult))
        return 1;

    target_row = b->r + mult;

    /* Get sticky column */
    if (b->sc_set)
        col = b->sc;
    else
        col = col_num(b);

    /* Try to go down */
    while (b->c != b->e && b->r != target_row)
        RIGHTCH(b);

    if (b->r != target_row) {
        /* Failed, go back */
        while (b->c != c_backup)
            LEFTCH(b);
        return 1;
    }

    /* Set the sticky column if not set (only set upon success) */
    if (!b->sc_set) {
        b->sc = col;
        b->sc_set = 1;
    }

    /* Try to move to the desired column */
    while (b->c != b->e && col && *b->c != '\n') {
        RIGHTCH(b);
        --col;
    }

    return 0;
}

void forward_word(struct buf *b, int mode, size_t mult)
{
    /*
     * Moves forward up to mult words. If mode is 0 then no editing
     * occurs. If mode is 1 then words will be converted to uppercase,
     * and if mode is 3 then words will be converted to lowercase.
     */
    int mod = 0;
    while (b->c != b->e && mult--) {
        /* Eat leading non-alphanumeric characters */
        while (b->c != b->e && ISASCII(*b->c) && !isalnum(*b->c))
            RIGHTCH(b);

        /* Convert letters to uppercase while in the alphanumeric word */
        while (b->c != b->e && ISASCII(*b->c) && isalnum(*b->c)) {
            switch (mode) {
            case 0:
                break;
            case 1:
                if (islower(*b->c)) {
                    *b->c = 'A' + *b->c - 'a';
                    mod = 1;
                }
                break;
            case 2:
                if (isupper(*b->c)) {
                    *b->c = 'a' + *b->c - 'A';
                    mod = 1;
                }
                break;
            }
            RIGHTCH(b);
        }
    }
    if (mod)
        SETMOD(b);
}

void backward_word(struct buf *b, size_t mult)
{
    /* Moves back a maximum of mult words */
    while (b->g != b->a && mult--) {
        /* Eat trailing non-alphanumeric characters */
        while (b->g != b->a && ISASCII(*(b->g - 1))
               && !isalnum(*(b->g - 1)))
            LEFTCH(b);
        /* Go to start of word */
        while (b->g != b->a && ISASCII(*(b->g - 1))
               && isalnum(*(b->g - 1))) {
            LEFTCH(b);
        }
    }
}

int insert_hex(struct buf *b, size_t mult)
{
    /*
     * Inserts a character mult times that was typed as its two digit
     * hexadecimal value.
     */
    int x;
    char ch = 0;
    size_t i = 2;
    while (i--) {
        x = getch();
        if (ISASCII(x) && isxdigit(x)) {
            if (islower(x))
                ch = ch * 16 + x - 'a' + 10;
            else if (isupper(x))
                ch = ch * 16 + x - 'A' + 10;
            else
                ch = ch * 16 + x - '0';
        } else {
            return 1;
        }
    }
    if (insert_ch(b, ch, mult))
        return 1;
    return 0;
}

void trim_clean(struct buf *b)
{
    /*
     * Trims trailing whitespace and deletes any character that is
     * not in {isgraph, ' ', '\t', '\n'}.
     */
    size_t r_backup = b->r;
    size_t col = col_num(b);
    int nl_found = 0;
    int at_eol = 0;
    int mod = 0;

    end_of_buf(b);

    /* Delete to end of text, sparing the first newline character */
    while (b->g != b->a) {
        LEFTCH(b);
        if (ISASCII(*b->c) && isgraph(*b->c)) {
            break;
        } else if (*b->c == '\n' && !nl_found) {
            nl_found = 1;
        } else {
            DELETECH(b);
            mod = 1;
        }
    }

    /* Process text, triming trailing whitespace */
    while (b->g != b->a) {
        LEFTCH(b);
        if (*b->c == '\n') {
            at_eol = 1;
        } else if (ISASCII(*b->c) && isgraph(*b->c)) {
            /* Never delete a graph character */
            at_eol = 0;
        } else if (at_eol) {
            /* Delete any remaining character at the end of the line */
            DELETECH(b);
            mod = 1;
        } else if (*b->c != ' ' && *b->c != '\t' && *b->c != '\n') {
            /* Delete any remaining characters inside the line */
            DELETECH(b);
            mod = 1;
        }
    }

    if (mod)
        SETMOD(b);

    /* Attempt to move back to old position */
    while (b->c != b->e && b->r != r_backup)
        RIGHTCH(b);
    while (b->c != b->e && col && *b->c != '\n') {
        RIGHTCH(b);
        --col;
    }
}

void str_buf(struct buf *b)
{
    /* Prepares a buffer so that b->c can be used as a string */
    end_of_buf(b);
    while (b->a != b->g) {
        LEFTCH(b);
        if (*b->c == '\0')
            DELETECH(b);
    }
}

void set_mark(struct buf *b)
{
    b->m = CURSOR_INDEX(b);
    b->mr = b->r;
    b->m_set = 1;
}

void clear_mark(struct buf *b)
{
    b->m = 0;
    b->mr = 1;
    b->m_set = 0;
}

int search(struct buf *b, char *p, size_t n)
{
    /* Forward search buffer b for memory p (n chars long) */
    char *q;
    if (b->c == b->e)
        return 1;
    if ((q = memmem(b->c + 1, b->e - b->c - 1, p, n)) == NULL)
        return 1;
    while (b->c != q)
        RIGHTCH(b);
    return 0;
}

int match_bracket(struct buf *b)
{
    /* Moves the cursor to the corresponding nested bracket */
    int right;
    char orig = *b->c;
    char target;
    size_t depth;
    char *backup = b->c;

    switch (orig) {
    case '(':
        target = ')';
        right = 1;
        break;
    case '{':
        target = '}';
        right = 1;
        break;
    case '[':
        target = ']';
        right = 1;
        break;
    case '<':
        target = '>';
        right = 1;
        break;
    case ')':
        target = '(';
        right = 0;
        break;
    case '}':
        target = '{';
        right = 0;
        break;
    case ']':
        target = '[';
        right = 0;
        break;
    case '>':
        target = '<';
        right = 0;
        break;
    default:
        return 1;
    }

    depth = 1;
    if (right) {
        while (b->c != b->e) {
            RIGHTCH(b);
            if (*b->c == orig)
                ++depth;
            if (*b->c == target)
                if (!--depth)
                    return 0;
        }
        while (b->c != backup)
            LEFTCH(b);
    } else {
        while (b->a != b->g) {
            LEFTCH(b);
            if (*b->c == orig)
                ++depth;
            if (*b->c == target)
                if (!--depth)
                    return 0;
        }
        while (b->c != backup)
            RIGHTCH(b);
    }

    return 1;
}

int copy_region(struct buf *b, struct buf *p)
{
    /*
     * Copies the region from buffer b into buffer p.
     * The cursor is moved to the end of buffer p first.
     */
    char *m_pointer;
    size_t s;
    /* Region does not exist */
    if (!b->m_set)
        return 1;
    /* Region is empty */
    if (b->m == CURSOR_INDEX(b))
        return 1;
    /* Make sure that the cursor is at the end of p */
    end_of_buf(p);
    m_pointer = INDEX_TO_POINTER(b, m);
    /* Mark before cursor */
    if (b->m < CURSOR_INDEX(b)) {
        s = b->g - m_pointer;
        if (s > GAPSIZE(p))
            if (grow_gap(p, s))
                return 1;
        /* Left of gap insert */
        memcpy(p->g, m_pointer, s);
        p->g += s;
        /* Adjust row number */
        p->r += b->r - b->mr;
    } else {
        /* Cursor before mark */
        s = m_pointer - b->c;
        if (s > GAPSIZE(p))
            if (grow_gap(p, s))
                return 1;
        /* Left of gap insert */
        memcpy(p->g, b->c, s);
        p->g += s;
        /* Adjust row number */
        p->r += b->mr - b->r;
    }
    clear_mark(b);
    SETMOD(p);
    return 0;
}

int delete_region(struct buf *b)
{
    /* Deletes the region */
    /* Region does not exist */
    if (!b->m_set)
        return 1;
    /* Region is empty */
    if (b->m == CURSOR_INDEX(b))
        return 1;
    /* Mark before cursor */
    if (b->m < CURSOR_INDEX(b)) {
        b->g = b->a + b->m;
        /* Adjust for removed rows */
        b->r = b->mr;
    } else {
        /* Cursor before mark */
        b->c = INDEX_TO_POINTER(b, m);
    }
    SETMOD(b);
    return 0;
}

int cut_region(struct buf *b, struct buf *p)
{
    /*
     * Copies the region from buffer b into buffer p.
     * The cursor is moved to the end of buffer p first.
     */
    if (copy_region(b, p))
        return 1;
    if (delete_region(b))
        return 1;
    return 0;
}

int paste(struct buf *b, struct buf *p, size_t mult)
{
    /*
     * Pastes (inserts) buffer p into buffer b mult times.
     * Moves the cursor to the end of buffer p first.
     */
    size_t num = mult;
    size_t s, ts;
    char *q;
    end_of_buf(p);
    s = p->g - p->a;
    if (!s)
        return 1;
    if (MOF(s, mult))
        return 1;
    ts = s * mult;
    if (ts > GAPSIZE(b))
        if (grow_gap(b, ts))
            return 1;
    q = b->g;
    while (num--) {
        /* Left of gap insert */
        memcpy(q, p->a, s);
        q += s;
    }
    b->g += ts;
    b->r += (p->r - 1) * mult;
    SETMOD(b);
    return 0;
}

int cut_to_eol(struct buf *b, struct buf *p)
{
    /* Cut to the end of the line */
    if (*b->c == '\n')
        return delete_ch(b, 1);
    set_mark(b);
    end_of_line(b);
    if (cut_region(b, p))
        return 1;
    return 0;
}

int cut_to_sol(struct buf *b, struct buf *p)
{
    /* Cut to the start of the line */
    set_mark(b);
    start_of_line(b);
    if (cut_region(b, p))
        return 1;
    return 0;
}

int filesize(char *fn, size_t * fs)
{
    /* Gets the filesize of a filename */
    struct stat st;
    if (stat(fn, &st))
        return 1;
    if (!S_ISREG(st.st_mode))
        return 1;
    if (st.st_size < 0)
        return 1;
    *fs = st.st_size;
    return 0;
}

int insert_file(struct buf *b, char *fn)
{
    /* Inserts a file into the righthand side of the gap */
    size_t fs;
    FILE *fp;
    if (filesize(fn, &fs))
        return 1;
    /* Nothing to do */
    if (!fs)
        return 0;
    if (fs > GAPSIZE(b))
        if (grow_gap(b, fs))
            return 1;
    if ((fp = fopen(fn, "r")) == NULL)
        return 1;
    /* Right of gap insert */
    if (fread(b->c - fs, 1, fs, fp) != fs) {
        fclose(fp);
        return 1;
    }
    b->c -= fs;
    SETMOD(b);
    return 0;
}

int write_file(struct buf *b)
{
    /* Writes a buffer to file */
    char *tmp_fn;
    FILE *fp;
    size_t n;
    /* No filename */
    if (b->fn == NULL || !strlen(b->fn))
        return 1;
    if (asprintf(&tmp_fn, "%s~", b->fn) == -1)
        return 1;
    if ((fp = fopen(tmp_fn, "w")) == NULL) {
        free(tmp_fn);
        return 1;
    }
    /* Before gap */
    n = b->g - b->a;
    if (fwrite(b->a, 1, n, fp) != n) {
        free(tmp_fn);
        fclose(fp);
        return 1;
    }
    /* After gap, excluding the last character */
    n = b->e - b->c;
    if (fwrite(b->c, 1, n, fp) != n) {
        free(tmp_fn);
        fclose(fp);
        return 1;
    }
    if (fclose(fp)) {
        free(tmp_fn);
        return 1;
    }
    /* Atomic */
    if (rename(tmp_fn, b->fn)) {
        free(tmp_fn);
        return 1;
    }
    free(tmp_fn);
    b->mod = 0;
    return 0;
}

int init_ncurses(void)
{
    /* Starts ncurses */
    if (initscr() == NULL)
        return 1;
    if (raw() == ERR) {
        endwin();
        return 1;
    }
    if (noecho() == ERR) {
        endwin();
        return 1;
    }
    if (keypad(stdscr, TRUE) == ERR) {
        endwin();
        return 1;
    }
    return 0;
}

void centre_cursor(struct buf *b, int text_height)
{
    /*
     * Sets draw start so that the cursor might be in the middle
     * of the screen. Does not handle long lines.
     */
    char *q;
    int up;
    up = text_height / 2;
    if (!up)
        up = 1;
    if (b->g == b->a) {
        b->d = 0;
        return;
    }
    q = b->g - 1;
    while (q != b->a && up) {
        if (*q == '\n')
            --up;
        --q;
    }

    /* Move to start of line */
    if (q != b->a)
        ++q;

    b->d = q - b->a;
}

int draw_screen(struct buf *b, struct buf *cl, int cl_active, int rv,
                int req_centre, int req_clear)
{
    /* Draws a buffer to the screen, including the command line buffer */
    char *q;
    int height, width;          /* Screen size */
    int cy, cx;                 /* Final cursor position */
    int y, x;                   /* Changing cursor position */
    int centred = 0;            /* Indicates if centreing has occurred */
    int rv_a = 0;               /* Return value of addch */
    char *sb;                   /* Status bar */

    getmaxyx(stdscr, height, width);

    /* Cursor is above the screen */
    if (req_centre || b->c < INDEX_TO_POINTER(b, d)) {
        centre_cursor(b, height >= 3 ? height - 2 : height);
        centred = 1;
    }

  draw_start:
    if (req_clear) {
        if (clear() == ERR)
            return 1;
        req_clear = 0;
    } else {
        if (erase() == ERR)
            return 1;
    }

    /* Commence from draw start */
    q = b->d + b->a;
    /* Start highlighting if mark is before draw start */
    if (b->m_set && b->m < b->d)
        if (standout() == ERR)
            return 1;

    /* Before gap */
    while (q != b->g) {
        /* Mark is on screen before cursor */
        if (b->m_set && q == INDEX_TO_POINTER(b, m))
            if (standout() == ERR)
                return 1;
        rv_a = addch(*q);
        getyx(stdscr, y, x);
        if ((height >= 3 && y >= height - 2) || rv_a == ERR) {
            /* Cursor out of text portion of the screen */
            if (!centred) {
                centre_cursor(b, height >= 3 ? height - 2 : height);
                centred = 1;
                goto draw_start;
            } else {
                /* Draw from the cursor */
                b->d = CURSOR_INDEX(b);
                goto draw_start;
            }
        }
        ++q;
    }

    /* Don't highlight the cursor itself */
    if (b->m_set) {
        if (INDEX_TO_POINTER(b, m) > b->c) {
            if (standout() == ERR)
                return 1;
        } else {
            /* Stop highlighting */
            if (standend() == ERR)
                return 1;
        }
    }

    /* Record cursor position */
    if (!cl_active)
        getyx(stdscr, cy, cx);
    /* After gap */
    q = b->c;
    while (q <= b->e) {
        /* Mark is after cursor */
        if (b->m_set && q == INDEX_TO_POINTER(b, m))
            if (standend() == ERR)
                return 1;
        rv_a = addch(*q);
        getyx(stdscr, y, x);
        if ((height >= 3 && y >= height - 2) || rv_a == ERR)
            break;
        ++q;
    }

    if (height >= 3) {
        /* Status bar */
        if (move(height - 2, 0) == ERR)
            return 1;
        /* Clear the line */
        /* if (clrtoeol() == ERR) return 1; */
        if (asprintf
            (&sb, "%c%c %s (%lu,%lu) %02X", rv ? '!' : ' ',
             b->mod ? '*' : ' ', b->fn, b->r, col_num(b),
             (unsigned char) *b->c) == -1)
            return 1;
        if (addnstr(sb, width) == ERR) {
            free(sb);
            return 1;
        }
        free(sb);
        /* Highlight status bar */
        if (mvchgat(height - 2, 0, width, A_STANDOUT, 0, NULL) == ERR)
            return 1;

        /* Command line buffer */
        if (move(height - 1, 0) == ERR)
            return 1;

      cl_draw_start:
        if (clrtobot() == ERR)
            return 1;

        /* Commence from draw start */
        q = cl->d + cl->a;
        /* Start highlighting if mark is before draw start */
        if (cl->m_set && cl->m < cl->d)
            if (standout() == ERR)
                return 1;

        /* Before gap */
        while (q != cl->g) {
            /* Mark is on screen before cursor */
            if (cl->m_set && q == INDEX_TO_POINTER(cl, m))
                if (standout() == ERR)
                    return 1;
            if (addch(*q) == ERR) {
                /* Draw from the cursor */
                cl->d = cl->g - cl->a;
                goto cl_draw_start;
            }
            ++q;
        }

        /* Don't highlight the cursor itself */
        if (cl->m_set) {
            if (INDEX_TO_POINTER(cl, m) > cl->c) {
                if (standout() == ERR)
                    return 1;
            } else {
                /* Stop highlighting */
                if (standend() == ERR)
                    return 1;
            }
        }

        /* Record cursor position */
        if (cl_active)
            getyx(stdscr, cy, cx);
        /* After gap */
        q = cl->c;
        while (q <= cl->e) {
            /* Mark is after cursor */
            if (cl->m_set && q == INDEX_TO_POINTER(cl, m))
                if (standend() == ERR)
                    return 1;
            if (addch(*q) == ERR)
                break;
            ++q;
        }
    }

    /* Position cursor */
    if (move(cy, cx) == ERR)
        return 1;
    if (refresh() == ERR)
        return 1;
    return 0;
}

int rename_buf(struct buf *b, char *new_fn)
{
    /* Sets the filename fn associated with buffer b to new_fn */
    char *t;
    if ((t = strdup(new_fn)) == NULL)
        return 1;
    free(b->fn);
    b->fn = t;
    return 0;
}

struct buf *new_buf(struct buf *b, char *fn)
{
    /*
     * Creates a new buffer to the right of buffer b in the doubly linked list
     * and sets the associated filename to fn. The file will be loaded into
     * the buffer if it exists.
     */
    struct buf *t;
    if ((t = init_buf()) == NULL)
        return NULL;

    if (fn != NULL) {
        if (!access(fn, F_OK)) {
            /* File exists */
            if (insert_file(t, fn)) {
                free_buf_list(t);
                return NULL;
            }
            /* Clear modification indicator */
            t->mod = 0;
        }
        if (rename_buf(t, fn)) {
            free_buf_list(t);
            return NULL;
        }
    }

    /* Link in the new node */
    if (b != NULL) {
        if (b->next != NULL) {
            b->next->prev = t;
            t->next = b->next;
        }
        b->next = t;
        t->prev = b;
    }
    return t;
}

struct buf *kill_buf(struct buf *b)
{
    /*
     * Kills (frees and unlinks) buffer b from the doubly linked list and
     * returns the buffer to the left (if present) or right (if present).
     */
    struct buf *t = NULL;
    if (b == NULL)
        return NULL;
    /* Unlink b */
    if (b->prev != NULL) {
        t = b->prev;
        b->prev->next = b->next;
    }
    if (b->next != NULL) {
        if (t == NULL)
            t = b->next;
        b->next->prev = b->prev;
    }
    /* Isolate b */
    b->prev = NULL;
    b->next = NULL;

    /* Free b */
    free_buf_list(b);

    return t;
}

int main(int argc, char **argv)
{
    int ret = 0;                /* Return value of text editor */
    int rv = 0;                 /* Internal function return value */
    int running = 1;            /* Text editor is on */
    int x;                      /* Read char */
    /* Current buffer of the doubly linked list of text buffers */
    struct buf *b = NULL;
    struct buf *cl = NULL;      /* Command line buffer */
    struct buf *z;              /* Shortcut to the active buffer */
    struct buf *p = NULL;       /* Paste buffer */
    int req_centre = 0;         /* User requests cursor centreing */
    int req_clear = 0;          /* User requests screen clearing */
    int cl_active = 0;          /* Command line is being used */
    /* Operation for which command line is being used */
    char operation = ' ';
    size_t mult;                /* Command multiplier */
    /* Persist the sticky column (used for repeated up or down) */
    int persist_sc = 0;
    int i;

    if (argc <= 1) {
        if ((b = new_buf(NULL, NULL)) == NULL) {
            ret = 1;
            goto clean_up;
        }
    } else {
        for (i = 1; i < argc; ++i) {
            if ((b = new_buf(b, *(argv + i))) == NULL) {
                ret = 1;
                goto clean_up;
            }
        }
    }

    if ((cl = init_buf()) == NULL) {
        ret = 1;
        goto clean_up;
    }

    if ((p = init_buf()) == NULL) {
        ret = 1;
        goto clean_up;
    }

    if (init_ncurses()) {
        ret = 1;
        goto clean_up;
    }

    while (running) {
      top:
        if (draw_screen(b, cl, cl_active, rv, req_centre, req_clear)) {
            ret = 1;
            goto clean_up;
        }
        /* Clear drawing options */
        req_centre = 0;
        req_clear = 0;
        /* Clear internal return value */
        rv = 0;

        /* Active buffer */
        if (cl_active)
            z = cl;
        else
            z = b;

        /* Update sticky column */
        if (persist_sc) {
            /* Turn off persist status, but do not clear sticky column yet */
            persist_sc = 0;
        } else {
            /* Clear stick column */
            z->sc = 0;
            z->sc_set = 0;
        }

        x = getch();

        mult = 0;
        if (x == CTRL('u')) {
            x = getch();
            while (ISASCII(x) && isdigit(x)) {
                if (MOF(mult, 10)) {
                    rv = 1;
                    goto top;
                }
                mult *= 10;
                if (AOF(mult, x - '0')) {
                    rv = 1;
                    goto top;
                }
                mult += x - '0';
                x = getch();
            }
        }
        /* mult cannot be zero */
        if (!mult)
            mult = 1;

        if (cl_active && x == '\n') {
            switch (operation) {
            case 'i':
                str_buf(cl);
                rv = insert_file(b, cl->c);
                break;
            case 's':
                start_of_buf(cl);
                rv = search(b, cl->c, cl->e - cl->c);
                break;
            case 'r':
                str_buf(cl);
                rv = rename_buf(b, cl->c);
                break;
            case 'n':
                str_buf(cl);
                if (new_buf(b, cl->c) == NULL) {
                    rv = 1;
                } else {
                    b = b->next;
                    rv = 0;
                }
                break;
            }
            cl_active = 0;
            operation = ' ';
            goto top;
        }

        switch (x) {
        case CTRL_SPC:
            set_mark(z);
            break;
        case CTRL('g'):
            if (z->m_set) {
                /* Clear mark if set */
                clear_mark(z);
            } else {
                /* Or quit the command line */
                cl_active = 0;
                operation = ' ';
            }
            break;
        case CTRL('h'):
        case KEY_BACKSPACE:
            rv = backspace_ch(z, mult);
            break;
        case CTRL('b'):
        case KEY_LEFT:
            rv = left_ch(z, mult);
            break;
        case CTRL('f'):
        case KEY_RIGHT:
            rv = right_ch(z, mult);
            break;
        case CTRL('p'):
        case KEY_UP:
            rv = up_line(z, mult);
            persist_sc = 1;
            break;
        case CTRL('n'):
        case KEY_DOWN:
            rv = down_line(z, mult);
            persist_sc = 1;
            break;
        case CTRL('a'):
            start_of_line(z);
            break;
        case CTRL('e'):
            end_of_line(z);
            break;
        case CTRL('d'):
        case KEY_DC:
            rv = delete_ch(z, mult);
            break;
        case CTRL('l'):
            req_centre = 1;
            req_clear = 1;
            break;
        case CTRL('s'):
            /* Search */
            DELETEBUF(cl);
            operation = 's';
            cl_active = 1;
            break;
        case CTRL('r'):
            /* Rename buffer */
            DELETEBUF(cl);
            operation = 'r';
            cl_active = 1;
            break;
        case CTRL('w'):
            DELETEBUF(p);
            rv = cut_region(z, p);
            break;
        case CTRL('c'):
            /* Cut, inserting at end of paste buffer */
            rv = cut_region(z, p);
            break;
        case CTRL('y'):
            rv = paste(z, p, mult);
            break;
        case CTRL('k'):
            DELETEBUF(p);
            rv = cut_to_eol(z, p);
            break;
        case CTRL('t'):
            trim_clean(z);
            break;
        case CTRL('q'):
            rv = insert_hex(z, mult);
            break;
        case CTRL('x'):
            switch (x = getch()) {
            case CTRL('c'):
                running = 0;
                break;
            case CTRL('s'):
                rv = write_file(z);
                break;
            case 'i':
                /* Insert file at the cursor */
                DELETEBUF(cl);
                operation = 'i';
                cl_active = 1;
                break;
            case CTRL('f'):
                /* New buffer */
                DELETEBUF(cl);
                operation = 'n';
                cl_active = 1;
                break;
            case KEY_LEFT:
                if (b->prev != NULL)
                    b = b->prev;
                else
                    rv = 1;
                break;
            case KEY_RIGHT:
                if (b->next != NULL)
                    b = b->next;
                else
                    rv = 1;
                break;
            }
            break;
        case ESC:
            switch (x = getch()) {
            case 'n':
                /* Search without editing the command line */
                start_of_buf(cl);
                rv = search(z, cl->c, cl->e - cl->c);
                break;
            case 'm':
                rv = match_bracket(z);
                break;
            case 'w':
                DELETEBUF(p);
                rv = copy_region(z, p);
                break;
            case 'c':
                /* Copy, inserting at end of paste buffer */
                rv = copy_region(z, p);
                break;
            case '!':
                /* Close editor if last buffer is killed */
                if ((b = kill_buf(b)) == NULL)
                    running = 0;
                break;
            case 'k':
                DELETEBUF(p);
                rv = cut_to_sol(z, p);
                break;
            case 'b':
                backward_word(z, mult);
                break;
            case 'f':
                forward_word(z, 0, mult);
                break;
            case 'u':
                forward_word(z, 1, mult);
                break;
            case 'l':
                forward_word(z, 2, mult);
                break;
            case '<':
                start_of_buf(z);
                break;
            case '>':
                end_of_buf(z);
                break;
            }
            break;
        default:
            if (ISASCII(x)
                && (isgraph(x) || x == ' ' || x == '\t' || x == '\n')) {
                rv = insert_ch(z, x, mult);
            }
            break;
        }
    }

  clean_up:
    free_buf_list(b);
    free_buf_list(cl);
    free_buf_list(p);

    if (stdscr != NULL)
        if (endwin() == ERR)
            ret = 1;

    return ret;
}
