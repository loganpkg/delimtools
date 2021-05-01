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
 */

#include <sys/stat.h>
#include <unistd.h>
#include <curses.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
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
#define DELETEBUF(b) do {b->g = b->a; b->c = b->e; b->r = 0; b->d = 0; \
                         b->m = 0; b->m_set = 0; b->mod = 1;} while (0)

/* Update settings when a buffer is modified */
#define SETMOD() do {b->m = 0; b->m_set = 0; b->mod = 1;} while (0)

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
    size_t d;                   /* Draw start index (ignores the gap) */
    size_t m;                   /* Mark index (ignores the gap) */
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
    b->d = 0;
    b->m = 0;
    b->m_set = 0;
    b->mod = 0;
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
    char *t;
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
    /* Copy text after the gap */
    memcpy(t + buf_size - 1 - (b->e - b->c + 1), b->c, b->e - b->c + 1);
    /* Update pointers, indices do not need to be changed */
    b->g = t + (b->g - b->a);
    b->c = t + buf_size - 1 - (b->e - b->c + 1);
    b->e = t + buf_size - 1;
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
    SETMOD();
    return 0;
}

int delete_ch(struct buf *b, size_t mult)
{
    /* Deletes mult chars in a buffer */
    if (mult > (size_t) (b->e - b->c))
        return 1;
    while (mult--)
        DELETECH(b);
    SETMOD();
    return 0;
}

int backspace_ch(struct buf *b, size_t mult)
{
    /* Backspaces mult chars in a buffer */
    if (mult > CURSOR_INDEX(b))
        return 1;
    while (mult--)
        BACKSPACECH(b);
    SETMOD();
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

void str_buf(struct buf *b)
{
    /* Prepares a buffer so that b->c can be used as a string */
    end_of_buf(b);
    while (b->a != b->g) {
        LEFTCH(b);
        if (*b->c == '\0')
            DELETECH(b);
    }
    *b->e = '\0';
}

void set_mark(struct buf *b)
{
    b->m = CURSOR_INDEX(b);
    b->m_set = 1;
}

void clear_mark(struct buf *b)
{
    b->m = 0;
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

int copy_region(struct buf *b, struct buf *p, int del)
{
    /*
     * Appends the region from buffer b into buffer p,
     * deleting the region if del is non-zero.
     */
    size_t s, r_start;
    char *q, ch, *m_pointer;
    if (!b->m_set)
        return 1;
    if (b->m == CURSOR_INDEX(b))
        return 1;
    if (b->m < CURSOR_INDEX(b)) {
        q = b->a + b->m;
        s = b->g - q;
        if (s > GAPSIZE(p))
            if (grow_gap(p, s))
                return 1;
        r_start = p->r;
        while (q != b->g) {
            ch = *q++;
            INSERTCH(p, ch);
        }
        if (del) {
            b->g = b->a + b->m;
            /* Adjust for removed rows */
            b->r -= p->r - r_start;
            SETMOD();
        }
    } else {
        q = b->c;
        m_pointer = INDEX_TO_POINTER(b, m);
        s = m_pointer - q;
        if (s > GAPSIZE(p))
            if (grow_gap(p, s))
                return 1;
        while (q != m_pointer) {
            ch = *q++;
            INSERTCH(p, ch);
        }
        if (del) {
            b->c = m_pointer;
            SETMOD();
        }
    }
    return 0;
}

int paste(struct buf *b, struct buf *p, size_t mult)
{
    /*
     * Pastes (inserts) buffer p into buffer b mult times.
     * Assumes the cursor is at the end of buffer p.
     */
    size_t s = p->g - p->a;
    char *q, ch;
    if (!s)
        return 1;
    if (MOF(s, mult))
        return 1;
    s *= mult;
    if (s > GAPSIZE(b))
        if (grow_gap(b, s))
            return 1;
    while (mult--) {
        q = p->a;
        while (q != p->g) {
            ch = *q++;
            INSERTCH(b, ch);
        }
    }
    SETMOD();
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
    if (fs > GAPSIZE(b))
        if (grow_gap(b, fs))
            return 1;
    if ((fp = fopen(fn, "r")) == NULL)
        return 1;
    if (fread(b->c - fs, 1, fs, fp) != fs) {
        fclose(fp);
        return 1;
    }
    b->c -= fs;
    SETMOD();
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
            (&sb, "%c%c %s (%lu) %02X", rv ? '!' : ' ', b->mod ? '*' : ' ',
             b->fn, b->r, (unsigned char) *b->c) == -1)
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
    char operation = ' ';       /* Operation for which command line is being used */
    size_t mult;                /* Command multiplier */
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
            case 'f':
                str_buf(cl);
                rv = insert_file(b, cl->c);
                break;
            case 's':
                start_of_buf(cl);
                rv = search(b, cl->c, cl->e - cl->c);
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
            clear_mark(z);
            break;
        case CTRL('h'):
        case KEY_BACKSPACE:
            rv = backspace_ch(z, mult);
            break;
        case KEY_LEFT:
            rv = left_ch(z, mult);
            break;
        case KEY_RIGHT:
            rv = right_ch(z, mult);
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
            break;
        case CTRL('s'):
            operation = 's';
            cl_active = 1;
            break;
        case CTRL('w'):
            DELETEBUF(p);
            rv = copy_region(z, p, 1);
            break;
        case CTRL('y'):
            rv = paste(z, p, mult);
            break;
        case CTRL('x'):
            switch (x = getch()) {
            case CTRL('c'):
                running = 0;
                break;
            case CTRL('s'):
                rv = write_file(z);
                break;
            case CTRL('f'):
                operation = 'f';
                cl_active = 1;
                break;
            case KEY_LEFT:
                if (b->prev != NULL)
                    b = b->prev;
                break;
            case KEY_RIGHT:
                if (b->next != NULL)
                    b = b->next;
                break;
            }
            break;
        case ESC:
            switch (x = getch()) {
            case 'l':
                req_centre = 1;
                req_clear = 1;
                break;
            case 's':
                start_of_buf(cl);
                rv = search(z, cl->c, cl->e - cl->c);
                break;
            case 'm':
                rv = match_bracket(z);
                break;
            case 'w':
                DELETEBUF(p);
                rv = copy_region(z, p, 0);
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
