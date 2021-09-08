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

#ifdef __linux__
#define _GNU_SOURCE
#endif

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#include <termios.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../../mods/gen/gen.h"
#include "../../mods/buf/buf.h"
#include "../../mods/minicurses/minicurses.h"
#include "../../mods/gapbuf/gapbuf.h"
#include "../../mods/fs/fs.h"

#define HELP char *help[] = { \
"spot keybindings", \
"^ means the control key, and ^[ is equivalent to the escape key.", \
"RK denotes the right key and LK the left key.", \
"Commands with descriptions ending with * take an optional command", \
"multiplier prefix ^U n (where n is a positive number).", \
"^[ ?   Display keybindings in new gap buffer", \
"^b     Backward char (left)*", \
"^f     Forward char (right)*", \
"^p     Previous line (up)*", \
"^n     Next line (down)*", \
"^h     Backspace*", \
"^d     Delete*", \
"^[ f   Forward word*", \
"^[ b   Backward word*", \
"^[ u   Uppercase word*", \
"^[ l   Lowercase word*", \
"^q hh  Quote two digit hexadecimal value*", \
"^a     Start of line (home)", \
"^e     End of line", \
"^[ <   Start of gap buffer", \
"^[ >   End of gap buffer", \
"^[ m   Match bracket", \
"^l     Level cursor and redraw screen", \
"^2     Set the mark", \
"^g     Clear the mark or escape the command line", \
"^x ^x  Switch cursor and mark", \
"^w     Wipe (cut) region", \
"^o     Wipe region appending on the paste gap buffer", \
"^[ w   Soft wipe (copy) region", \
"^[ o   Soft wipe region appending on the paste gap buffer", \
"^k     Kill (cut) to end of line", \
"^[ k   Kill (cut) to start of line", \
"^y     Yank (paste)", \
"^t     Trim trailing whitespace and clean", \
"^s     Search", \
"^[ n   Search without editing the command line", \
"^r     Regex replace region, where the first character is the delimiter, e.g:", \
"           |find|replace", \
"^[ r   Regex replace region, newline insensitive", \
"^x i   Insert file at cursor", \
"^x ^F  Open file in new gap buffer", \
"^[ =   Rename gap buffer", \
"^x ^s  Save current gap buffer", \
"^x LK  Move left one gap buffer", \
"^x RK  Move right one gap buffer", \
"^[ !   Close current gap buffer without saving", \
"^x ^c  Close editor without saving any gap buffers", \
NULL \
}

/* Initial gap buffer size */
#define INIT_GAPBUF_SIZE BUFSIZ

/* Global variable */
WINDOW *stdscr = NULL;

int insert_hex(struct gapbuf *b, size_t mult)
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
        if (is_ascii(x) && isxdigit(x)) {
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

int init_ncurses(void)
{
    /* Starts ncurses */
    if (initscr() == NULL)
        return 1;

    return 0;
}

int insert_help_line(struct gapbuf *b, char *str)
{
    char ch;
    while ((ch = *str++))
        if (insert_ch(b, ch, 1))
            return 1;
    if (insert_ch(b, '\n', 1))
        return 1;
    return 0;
}


void centre_cursor(struct gapbuf *b, int text_height)
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
    while (q != b->a) {
        if (*q == '\n' && !--up)
            break;
        --q;
    }

    /* Move to start of line */
    if (q != b->a)
        ++q;

    b->d = q - b->a;
}

#define DISPLAYCH(ch) (isgraph(ch) || ch == ' ' || ch == '\t' || ch == '\n' \
    ? ch : (ch == '\0' ? '~' : '?'))

#define DRAWCH do { \
    ch = *q; \
    ch = DISPLAYCH(ch); \
    printch(ch); \
} while(0)

int draw_gapbuf(struct gapbuf *b, int y_top, int y_bottom, int ed,
                int req_centre, int *cy, int *cx)
{
    /*
     * Draws a gap buffer to a screen area bounded by y_top and y_bottom,
     * the inclusive top and bottom y indices, respectively.
     */
    char *q, ch;
    int y;                      /* Changing cursor vertical position */
    int ret = 0;                /* Macro "return value" */
    int centred = 0;            /* Indicates if centreing has occurred */

  draw_start:
    move_cursor(y_top, 0);
    if (ret)
        return 1;

    /*
     * Only clear down on first attempt if needed. For example, this is not
     * needed if erase() was just called.
     */
    if (ed) {
        erase_down();
        if (ret)
            return 1;
    }
    /* Always clean down on subsequent attempts */
    ed = 1;

    /* Cursor is above draw start */
    if (req_centre || b->c < INDEX_TO_POINTER(b, d)) {
        centre_cursor(b, y_bottom - y_top + 1);
        req_centre = 0;
        centred = 1;
    }

    /* Commence from draw start */
    q = b->d + b->a;
    /* Start highlighting if mark is before draw start */
    if (b->m_set && b->m < b->d)
        standout();

    /* Before gap */
    while (q != b->g) {
        /* Mark is on screen before cursor */
        if (b->m_set && q == INDEX_TO_POINTER(b, m))
            standout();

        DRAWCH;
        get_cursor_y(y);
        if (y > y_bottom || ret) {
            /* Cursor out of text portion of the screen */
            if (!centred) {
                centre_cursor(b, y_bottom - y_top + 1);
                centred = 1;
                goto draw_start;
            } else {
                /*
                 * Do no attempt to centre again, draw from the cursor instead.
                 * This is to accommodate lines that are very long.
                 */
                b->d = CURSOR_INDEX(b);
                goto draw_start;
            }
        }
        ++q;
    }

    /* Do not highlight the cursor itself */
    if (b->m_set)
        INDEX_TO_POINTER(b, m) > b->c ? standout() : standend();

    /* Record cursor position */
    get_cursor(*cy, *cx);
    /* After gap */
    q = b->c;
    while (q <= b->e) {
        /* Mark is after cursor */
        if (b->m_set && q == INDEX_TO_POINTER(b, m))
            standend();

        DRAWCH;
        get_cursor_y(y);
        if (y > y_bottom || ret)
            break;
        ++q;
    }

    return 0;
}

int draw_screen(struct gapbuf *b, struct gapbuf *cl, int cl_active,
                char **sb, size_t * sb_s, int rv, int *req_centre,
                int *req_clear)
{
    /* Draws the text and command line gap buffers to the screen */
    int h, w;                   /* Screen size */
    size_t width;               /* Screen width as size_t */
    int cl_cy, cl_cx;           /* Cursor position in command line */
    int cy = 0, cx = 0;         /* Final cursor position */
    int ret = 0;                /* Macro "return value" */

    if (*req_clear) {
        if (clear())
            return 1;
        *req_clear = 0;
    } else {
        if (erase())
            return 1;
    }

    /* erase() updates stdscr->h and stdscr->w, so best to call afterwards */
    get_max(h, w);

    if (h < 1 || w < 1)
        return 1;
    width = (size_t) w;

    /* Draw the text portion of the screen */
    if (draw_gapbuf
        (b, 0, h >= 3 ? h - 1 - 2 : h - 1, 0, *req_centre, &cy, &cx))
        return 1;
    *req_centre = 0;

    if (h >= 3) {
        /* Status bar */
        move_cursor(h - 2, 0);
        if (ret)
            return 1;

        /* sb_s needs to include the '\0' terminator */
        if (width >= *sb_s) {
            free(*sb);
            *sb = NULL;
            *sb_s = 0;
            if (aof(width, 1))
                return 1;
            if ((*sb = malloc(width + 1)) == NULL)
                return 1;
            *sb_s = width + 1;
        }
        if (snprintf
            (*sb, *sb_s, "%c%c %s (%lu,%lu) %02X", rv ? '!' : ' ',
             b->mod ? '*' : ' ', b->fn,
             (unsigned long) (cl_active ? cl->r : b->r),
             (unsigned long) col_num(cl_active ? cl : b),
             (unsigned char) (cl_active ? *cl->c : *b->c)) < 0)
            return 1;
        if (addnstr(*sb, w))
            return 1;
        /* Highlight status bar */
        move_cursor(h - 2, 0);
        if (ret)
            return 1;
        standout_to_eol();
        if (ret)
            return 1;

        /* Draw the command line */
        if (draw_gapbuf(cl, h - 1, h - 1, 0, 0, &cl_cy, &cl_cx))
            return 1;
        if (cl_active) {
            cy = cl_cy;
            cx = cl_cx;
        }
    }

    /* Position cursor */
    move_cursor((size_t) cy, (size_t) cx);
    if (ret)
        return 1;
    if (refresh())
        return 1;
    return 0;
}

int rename_gapbuf(struct gapbuf *b, char *new_fn)
{
    /* Sets the filename fn associated with gap buffer b to new_fn */
    char *t;
    if ((t = strdup(new_fn)) == NULL)
        return 1;
    free(b->fn);
    b->fn = t;
    return 0;
}

struct gapbuf *new_gapbuf(struct gapbuf *b, char *fn)
{
    /*
     * Creates a new gap buffer to the right of gap buffer b in the doubly
     * linked list and sets the associated filename to fn. The file will be
     * loaded into the gap buffer if it exists.
     */
    struct gapbuf *t;
    if ((t = init_gapbuf(INIT_GAPBUF_SIZE)) == NULL)
        return NULL;

#ifndef F_OK
#define F_OK 0
#endif

    if (fn != NULL) {
        if (exists(fn)) {
            /* File exists */
            if (insert_file(t, fn)) {
                free_gapbuf_list(t);
                return NULL;
            }
            /* Clear modification indicator */
            t->mod = 0;
        }
        if (rename_gapbuf(t, fn)) {
            free_gapbuf_list(t);
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

struct gapbuf *kill_gapbuf(struct gapbuf *b)
{
    /*
     * Kills (frees and unlinks) gap buffer b from the doubly linked list and
     * returns the gap buffer to the left (if present) or right (if present).
     */
    struct gapbuf *t = NULL;
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
    free_gapbuf_list(b);

    return t;
}

int main(int argc, char **argv)
{
    int ret = 0;                /* Return value of text editor */
    int rv = 0;                 /* Internal function return value */
    int running = 1;            /* Text editor is on */
    int x;                      /* Read char */
    /* Current gap buffer of the doubly linked list of text gap buffers */
    struct gapbuf *b = NULL;
    struct gapbuf *cl = NULL;   /* Command line gap buffer */
    struct gapbuf *z;           /* Shortcut to the active gap buffer */
    struct gapbuf *p = NULL;    /* Paste gap buffer */
    char *sb = NULL;            /* Status bar */
    size_t sb_s = 0;            /* Status bar size */
    int req_centre = 0;         /* User requests cursor centreing */
    int req_clear = 0;          /* User requests screen clearing */
    int cl_active = 0;          /* Command line is being used */
    /* Operation for which command line is being used */
    char operation = ' ';
    size_t mult;                /* Command multiplier */
    /* Persist the sticky column (used for repeated up or down) */
    int persist_sc = 0;
    int i;
    /* For displaying keybindings help */
    HELP;
    char **h;

#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1)
        return 1;
    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        return 1;
    if (_setmode(_fileno(stderr), _O_BINARY) == -1)
        return 1;
#endif

    if (argc <= 1) {
        if ((b = new_gapbuf(NULL, NULL)) == NULL)
            quit();
    } else {
        for (i = 1; i < argc; ++i) {
            if ((b = new_gapbuf(b, *(argv + i))) == NULL)
                quit();
        }
        /* Move left to the first file specified */
        while (b->prev != NULL)
            b = b->prev;
    }

    if ((cl = init_gapbuf(INIT_GAPBUF_SIZE)) == NULL)
        quit();
    if ((p = init_gapbuf(INIT_GAPBUF_SIZE)) == NULL)
        quit();

    if (init_ncurses())
        quit();

    while (running) {
      top:
        if (draw_screen
            (b, cl, cl_active, &sb, &sb_s, rv, &req_centre, &req_clear))
            quit();

        /* Clear internal return value */
        rv = 0;

        /* Active gap buffer */
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
        if (x == c('u')) {
            x = getch();
            while (is_ascii(x) && isdigit(x)) {
                if (mof(mult, 10)) {
                    rv = 1;
                    goto top;
                }
                mult *= 10;
                if (aof(mult, x - '0')) {
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

        /* Map Carriage Returns to Line Feeds */
        if (x == '\r' || x == KEY_ENTER)
            x = '\n';

        if (cl_active && x == '\n') {
            switch (operation) {
            case 'i':
                str_gapbuf(cl);
                rv = insert_file(b, cl->c);
                break;
            case 's':
                start_of_gapbuf(cl);
                rv = search(b, cl->c, cl->e - cl->c);
                break;
            case 'x':
                str_gapbuf(cl);
                rv = regex_replace_region(b, cl->c, 1);
                break;
            case 'r':
                str_gapbuf(cl);
                rv = regex_replace_region(b, cl->c, 0);
                break;
            case '=':
                str_gapbuf(cl);
                rv = rename_gapbuf(b, cl->c);
                break;
            case 'f':
                str_gapbuf(cl);
                if (new_gapbuf(b, cl->c) == NULL) {
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
        case C_2:
            set_mark(z);
            break;
        case c('g'):
            if (z->m_set) {
                /* Clear mark if set */
                clear_mark(z);
            } else {
                /* Or quit the command line */
                cl_active = 0;
                operation = ' ';
            }
            break;
        case c('h'):
        case 127:
        case KEY_BACKSPACE:
            rv = backspace_ch(z, mult);
            break;
        case c('b'):
        case KEY_LEFT:
            rv = left_ch(z, mult);
            break;
        case c('f'):
        case KEY_RIGHT:
            rv = right_ch(z, mult);
            break;
        case c('p'):
        case KEY_UP:
            rv = up_line(z, mult);
            persist_sc = 1;
            break;
        case c('n'):
        case KEY_DOWN:
            rv = down_line(z, mult);
            persist_sc = 1;
            break;
        case c('a'):
        case KEY_HOME:
            start_of_line(z);
            break;
        case c('e'):
        case KEY_END:
            end_of_line(z);
            break;
        case c('d'):
        case KEY_DC:
            rv = delete_ch(z, mult);
            break;
        case c('l'):
            /* Levels the text portion of the screen */
            req_centre = 1;
            req_clear = 1;
            break;
        case c('s'):
            /* Search */
            DELETEGAPBUF(cl);
            operation = 's';
            cl_active = 1;
            break;
        case c('r'):
            /* Regex replace region, newline sensitive */
            DELETEGAPBUF(cl);
            operation = 'x';
            cl_active = 1;
            break;
        case c('w'):
            DELETEGAPBUF(p);
            rv = cut_region(z, p);
            break;
        case c('o'):
            /* Cut, inserting at end of paste gap buffer */
            rv = cut_region(z, p);
            break;
        case c('y'):
            rv = paste(z, p, mult);
            break;
        case c('k'):
            DELETEGAPBUF(p);
            rv = cut_to_eol(z, p);
            break;
        case c('t'):
            trim_clean(z);
            break;
        case c('q'):
            rv = insert_hex(z, mult);
            break;
        case c('x'):
            switch (x = getch()) {
            case c('c'):
                running = 0;
                break;
            case c('s'):
                rv = write_file(z);
                break;
            case 'i':
                /* Insert file at the cursor */
                DELETEGAPBUF(cl);
                operation = 'i';
                cl_active = 1;
                break;
            case c('f'):
                /* New gap buffer */
                DELETEGAPBUF(cl);
                operation = 'f';
                cl_active = 1;
                break;
            case c('x'):
                switch_cursor_and_mark(z);
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
            case '=':
                /* Rename gap buffer */
                DELETEGAPBUF(cl);
                operation = '=';
                cl_active = 1;
                break;
            case 'n':
                /* Search without editing the command line */
                if (!cl_active) {
                    start_of_gapbuf(cl);
                    rv = search(z, cl->c, cl->e - cl->c);
                } else {
                    rv = 1;
                }
                break;
            case 'm':
                rv = match_bracket(z);
                break;
            case 'w':
                DELETEGAPBUF(p);
                rv = copy_region(z, p);
                clear_mark(z);
                break;
            case 'o':
                /* Copy, inserting at end of paste gap buffer */
                rv = copy_region(z, p);
                break;
            case '!':
                /* Close editor if last gap buffer is killed */
                if ((b = kill_gapbuf(b)) == NULL)
                    running = 0;
                break;
            case 'k':
                DELETEGAPBUF(p);
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
            case 'r':
                /* Regex replace region, newline insensitive */
                DELETEGAPBUF(cl);
                operation = 'r';
                cl_active = 1;
                break;
            case '<':
                start_of_gapbuf(z);
                break;
            case '>':
                end_of_gapbuf(z);
                break;
            case '?':
                if (new_gapbuf(b, NULL) == NULL) {
                    rv = 1;
                } else {
                    b = b->next;
                    h = help;
                    while (*h != NULL)
                        if ((rv = insert_help_line(b, *h++)))
                            break;
                    if (!rv)
                        start_of_gapbuf(b);
                }
                break;
            }
            break;
        default:
            if (is_ascii(x)
                && (isgraph(x) || x == ' ' || x == '\t' || x == '\n')) {
                rv = insert_ch(z, x, mult);
            }
            break;
        }
    }

  clean_up:
    free_gapbuf_list(b);
    free_gapbuf_list(cl);
    free_gapbuf_list(p);
    free(sb);
    if (stdscr != NULL)
        if (endwin())
            ret = 1;

    return ret;
}
