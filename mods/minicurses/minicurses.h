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

#ifndef MINICURSES_H
#define MINICURSES_H

/* Number of spaces used to display a tab (must be at least 1) */
#define TABSIZE 4

#define KEY_ENTER 343
#define KEY_DC 330
#define KEY_BACKSPACE 263
#define KEY_LEFT 260
#define KEY_RIGHT 261
#define KEY_UP 259
#define KEY_DOWN 258
#define KEY_HOME 262
#define KEY_END 360

struct graph {
    char *ns;                   /* Next screen (virtual) */
    char *cs;                   /* Current screen (virtual) */
    size_t vms;                 /* Virtual memory size */
    size_t h;                   /* Screen height (real) */
    size_t w;                   /* Screen width (real) */
    size_t sa;                  /* Screen area (real) */
    size_t v;                   /* Virtual index */
    int hard;                   /* Clear the physical screen */
    int iv;                     /* Inverse video mode (virtual) */
    int phy_iv;                 /* Mirrors the physical inverse video mode */
    struct buf *input;          /* Keyboard input buffer */
#ifndef _WIN32
    struct termios t_orig;      /* Original terminal attributes */
#endif
};

typedef struct graph WINDOW;

/* Declare this in the application */
extern WINDOW *stdscr;

/* Index starts from zero. Top left is (0, 0). Sets ret. */
#define move_cursor(y, x) do { \
    if ((size_t) (y) < stdscr->h && (size_t) (x) < stdscr->w) { \
        stdscr->v = (y) * stdscr->w + (x); \
        ret = 0; \
    } else { \
        ret = 1; \
    } \
} while (0)

#define erase_down() do { \
    if (stdscr->v < stdscr->sa) { \
        memset(stdscr->ns + stdscr->v, ' ', stdscr->sa - stdscr->v); \
        ret = 0; \
    } else { \
        ret = 1; \
    } \
} while (0)

#define standout_to_eol() do { \
    if (stdscr->v < stdscr->sa) { \
        *(stdscr->ns + stdscr->v) \
            = (char) (*(stdscr->ns + stdscr->v) | 0x80); \
        ++stdscr->v; \
        while (stdscr->v < stdscr->sa && stdscr->v % stdscr->w) { \
            *(stdscr->ns + stdscr->v) \
                = (char) (*(stdscr->ns + stdscr->v) | 0x80); \
            ++stdscr->v; \
        } \
        ret = 0; \
    } else { \
        ret = 1; \
    } \
} while (0)

#define get_cursor_y(y) y = stdscr->v / stdscr->w

#define get_cursor_x(x) x = stdscr->v % stdscr->w

#define get_cursor(y, x) do { \
    get_cursor_y(y); \
    get_cursor_x(x); \
} while (0)

#define get_max(y, x) do { \
    y = stdscr->h; \
    x = stdscr->w; \
} while (0)

#define standout() (stdscr->iv = 1)
#define standend() (stdscr->iv = 0)

/* If inverse video mode is on then set the highest bit on the char */
#define ivch(ch) (stdscr->iv ? (char) ((ch) | 0x80) : (ch))

#define ivon(ch) ((ch) & 0x80)

/*
 * Prints a character to the virtual screen.
 * Evaluates ch more than once. Sets ret.
 */
#define printch(ch) do { \
    if (stdscr->v < stdscr->sa) { \
        if (isgraph(ch) || ch == ' ') { \
            *(stdscr->ns + stdscr->v++) = ivch(ch); \
        } else if (ch == '\n') { \
            *(stdscr->ns + stdscr->v++) = ivch(' '); \
            if (stdscr->v % stdscr->w) \
                stdscr->v = (stdscr->v / stdscr->w + 1) * stdscr->w; \
        } else if (ch == '\t') { \
            memset(stdscr->ns + stdscr->v, ivch(' '), TABSIZE); \
            stdscr->v += TABSIZE; \
        } else { \
            *(stdscr->ns + stdscr->v++) = ivch('?'); \
        } \
        if (stdscr->v >= stdscr->sa) \
            ret = 1; \
        else \
            ret = 0; \
    } else { \
        ret = 1; \
    } \
} while (0)

#define ungetch(ch) unget_ch(stdscr->input, ch)

int addnstr(char *str, int n);
int erase(void);
int clear(void);
int endwin(void);
WINDOW *initscr(void);
int refresh(void);
int getch(void);

#endif
