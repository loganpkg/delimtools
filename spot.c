/*
 * Copyright (c) 2021, 2022 Logan Ryan McLintock
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
 * References:
 *
 * Section 4.3 External Variables:
 * Brian W. Kernighan and Dennis M. Ritchie, The C Programming Language,
 *     Second Edition, Prentice Hall Software Series, New Jersey, 1988.
 * Clovis L. Tondo and Scott E. Gimpel, The C Answer Book, Second Edition,
 *     PTR Prentice Hall Software Series, New Jersey, 1989.
 *
 * 9.2 Regular Expressions:
 * Brian W. Kernighan and Rob Pike, The Practice of Programming,
 *     Addison Wesley Longman, Massachusetts, 1999.
 *
 * Brian Kernighan, Beautiful Code: Chapter 1: A Regular Expression Matcher,
 *     O'Reilly Media, California, 2007.
 */

#ifdef __linux__
/* For cfmakeraw */
#define _DEFAULT_SOURCE
#endif

#ifdef _WIN32
/* For _getch */
#include <conio.h>
/* For _O_BINARY */
#include <fcntl.h>
/* For _setmode and _isatty */
#include <io.h>
/*
 * For GetStdHandle, GetConsoleScreenBufferInfo, GetConsoleMode,
 * and SetConsoleMode.
 */
#include <windows.h>
#else
/* For struct winsize, ioctl, and TIOCGWINSZ */
#include <sys/ioctl.h>
/* For struct termios, tcgetattr, tcsetattr, TCSANOW, and cfmakeraw */
#include <termios.h>
/* For STDOUT_FILENO and isatty */
#include <unistd.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>

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
"^s     Forward search", \
"^z     Regex forward search", \
"^[ z   Regex forward search, newline insensitive", \
"^[ n   Repeat the last search type without editing the command line", \
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

#ifdef _WIN32
#define ssize_t SSIZE_T
#endif

/* ************************************************************************** */
/* Configuration: */
#define INIT_BUF_SIZE 512
#define INIT_GAPBUF_SIZE BUFSIZ
/* Number of spaces used to display a tab (must be at least 1) */
#define TABSIZE 4
/* ************************************************************************** */

/* size_t Addition OverFlow test */
#define aof(a, b) ((a) > SIZE_MAX - (b))
/* size_t Multiplication OverFlow test */
#define mof(a, b) ((a) && (b) > SIZE_MAX / (a))

/* Takes signed input */
#define is_ascii(x) ((x) >= 0 && (x) <= 127)

/* Converts a lowercase letter to the corresponding control value */
#define c(l) ((l) - 'a' + 1)

/* Control 2 (control spacebar or control @ may work too) */
#define C_2 0

/* Escape key */
#define ESC 27

#define UCHAR_NUM (UCHAR_MAX + 1)
#define ASCII_NUM 128

#define quit() do { \
    ret = 1; \
    goto clean_up; \
} while (0)

/* ************************************************************************** */

struct buf {
    char *a;
    size_t i;
    size_t s;
};

#define buf_free_size(b) (b->s - b->i)

/*
 * put_ch is the same as unget_ch, just a different name depending on the
 * context. put_ch is used for output and unget_ch is used for input.
 * unget_ch is a buf function.
 */
#define put_ch unget_ch

/* ************************************************************************** */

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
    int tty;                    /* Input is from a terminal */
    /* Original terminal attributes */
#ifdef _WIN32
    HANDLE h_out;
    DWORD t_orig;
#else
    struct termios t_orig;
#endif
};

/* Global variable */
struct graph *stdscr = NULL;

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

#define DISPLAYCH(ch) (isgraph(ch) || ch == ' ' || ch == '\t' || ch == '\n' \
    ? ch : (ch == '\0' ? '~' : '?'))

#define DRAWCH do { \
    ch = *q; \
    ch = DISPLAYCH(ch); \
    printch(ch); \
} while(0)

/* Buffered getch with no key interpretation */
#ifdef _WIN32
#define getch_nk() (stdscr->input->i ? *(stdscr->input->a + --stdscr->input->i) \
    : (stdscr->tty ? _getch() : getchar()))
#else
#define getch_nk() (stdscr->input->i ? *(stdscr->input->a + --stdscr->input->i) \
    : getchar())
#endif

#define ungetch(ch) unget_ch(stdscr->input, ch)

/* ANSI escape sequences */
#define phy_clear_screen() printf("\033[2J\033[1;1H")

/* Index starts at one. Top left is (1, 1) */
#define phy_move_cursor(y, x) printf("\033[%lu;%luH", (unsigned long) (y), \
    (unsigned long) (x))

#define phy_attr_off() printf("\033[m")

#define phy_inverse_video() printf("\033[7m")

/* ************************************************************************** */

/* Calculates the gap size */
#define GAPSIZE(b) ((size_t) (b->c - b->g))

/* Update settings when a gap buffer is modified */
#define SETMOD(b) do {b->m = 0; b->mr = 1; b->m_set = 0; b->mod = 1;} while (0)

/* No out of bounds or gap size checks are performed */
#define INSERTCH(b, x) do {*b->g++ = x; if (x == '\n') ++b->r;} while(0)
#define DELETECH(b) ++b->c
#define BACKSPACECH(b) if (*--b->g == '\n') --b->r
#define LEFTCH(b) do {*--b->c = *--b->g; if (*b->c == '\n') --b->r;} while(0)
#define RIGHTCH(b) do {if (*b->c == '\n') ++b->r; *b->g++ = *b->c++;} while (0)

/* gap buffer */
struct gapbuf {
    struct gapbuf *prev;        /* Previous gap buffer node */
    char *fn;                   /* Filename */
    char *a;                    /* Start of gap buffer */
    char *g;                    /* Start of gap */
    char *c;                    /* Cursor (to the right of the gap) */
    char *e;                    /* End of gap buffer */
    size_t r;                   /* Row number (starting from 1) */
    size_t sc;                  /* Sticky column number (starting from 0) */
    int sc_set;                 /* Sticky column is set */
    size_t d;                   /* Draw start index (ignores the gap) */
    size_t m;                   /* Mark index (ignores the gap) */
    size_t mr;                  /* Row number at the mark */
    int m_set;                  /* Mark is set */
    int mod;                    /* Gap buffer text has been modified */
    struct gapbuf *next;        /* Next gap buffer node */
};

/* Converts the cursor pointer to an index */
#define CURSOR_INDEX(b) ((size_t) (b->g - b->a))

/* Converts an index to a pointer */
#define INDEX_TO_POINTER(b, i) (b->a + b->i < b->g ? b->a + b->i \
    : b->c + b->i - (b->g - b->a))

/* Delete gap buffer */
#define DELETEGAPBUF(b) do {b->g = b->a; b->c = b->e; b->r = 1; b->d = 0; \
    b->m = 0; b->mr = 1; b->m_set = 0; b->mod = 1;} while (0)

/* ************************************************************************** */

#define match_atom(atom, ch) (atom->set[(unsigned char) ch] != atom->negate)

#define NUM_CAP_GRP 10

struct atom {
    char set[UCHAR_NUM];        /* Character set */
    char negate;                /* Negate the character set */
    char end;                   /* End of atom sequence */
    ssize_t min_occ;            /* Minimum number of occurrences required */
    ssize_t max_occ;            /* Maximum (as above). -1 means no limit. */
    size_t num;                 /* Number of matches. Default is 1. */
};

struct hook {
    char start;                 /* ^ */
    char end;                   /* $ */
};

/*
 * Capture group.
 * An atom_start or atom_end value of -1 signifys that it is not used.
 */
struct cap_grp {
    ssize_t atom_start;         /* Start index of capture group (inclusive) */
    ssize_t atom_end;           /* End index of capture group (exclusive) */
    char *p;                    /* Pointer to start of captured text */
    size_t len;                 /* Length of captured text */
};

#define cr_bail() do { \
    free(cr); \
    return NULL; \
} while (0)

/* ************************************************************************** */

char *xstrdup(char *str)
{
    /* Overflow is not possible as the original string is already in memmory */
    char *new_str;
    size_t len = strlen(str);
    if ((new_str = malloc(len + 1)) == NULL)
        return NULL;
    memcpy(new_str, str, len + 1);
    return new_str;
}

int filesize(char *fn, size_t * fs)
{
    /*
     * Gets the filesize of a filename.
     * Returns 2 if file does not exist or 1 for other errors.
     */
    FILE *fp;
    long s;

    errno = 0;
    if ((fp = fopen(fn, "rb")) == NULL) {
        if (errno == ENOENT)
            return 2;
        else
            return 1;
    }
    if (fseek(fp, 0L, SEEK_END)) {
        fclose(fp);
        return 1;
    }
    if ((s = ftell(fp)) == -1) {
        fclose(fp);
        return 1;
    }

    if (fclose(fp))
        return 1;

    *fs = s;

    return 0;
}

/* ************************************************************************** */

struct buf *init_buf(size_t init_buf_size)
{
    struct buf *b;
    if ((b = malloc(sizeof(struct buf))) == NULL)
        return NULL;
    if ((b->a = malloc(init_buf_size)) == NULL) {
        free(b);
        return NULL;
    }
    b->s = init_buf_size;
    b->i = 0;
    return b;
}

void free_buf_wrapping(struct buf *b)
{
    /* Just frees the struct, not the mem inside */
    if (b != NULL)
        free(b);
}

void free_buf(struct buf *b)
{
    if (b != NULL) {
        free(b->a);
        free(b);
    }
}

int grow_buf(struct buf *b, size_t will_use)
{
    char *t;
    size_t new_s;
    /* Gap is big enough, nothing to do */
    if (will_use <= buf_free_size(b))
        return 0;
    if (mof(b->s, 2))
        return 1;
    new_s = b->s * 2;
    if (aof(new_s, will_use))
        return 1;
    new_s += will_use;
    if ((t = realloc(b->a, new_s)) == NULL)
        return 1;
    b->a = t;
    b->s = new_s;
    return 0;
}

int unget_ch(struct buf *b, int ch)
{
    if (b->i == b->s && grow_buf(b, 1))
        return 1;
    *(b->a + b->i++) = ch;
    return 0;
}

int put_str(struct buf *b, char *str)
{
    size_t len;
    if (str == NULL)
        return 1;
    if (!(len = strlen(str)))
        return 0;
    if (len > buf_free_size(b) && grow_buf(b, len))
        return 1;
    memcpy(b->a + b->i, str, len);
    b->i += len;
    return 0;
}

int put_mem(struct buf *b, char *mem, size_t mem_s)
{
    if (mem == NULL)
        return 1;
    if (!mem_s)
        return 0;
    if (mem_s > buf_free_size(b) && grow_buf(b, mem_s))
        return 1;
    memcpy(b->a + b->i, mem, mem_s);
    b->i += mem_s;
    return 0;
}

/* ************************************************************************** */

char *memmatch(char *big, size_t big_len, char *little, size_t little_len)
{
    /*
     * Quick Search algorithm:
     * Daniel M. Sunday, A Very Fast Substring Search Algorithm,
     *     Communications of the ACM, Vol.33, No.8, August 1990.
     */
    unsigned char bad[UCHAR_NUM], *pattern, *q, *q_stop, *q_check;
    size_t to_match, j;
    if (!little_len)
        return big;
    if (little_len > big_len)
        return NULL;
    for (j = 0; j < UCHAR_NUM; ++j)
        bad[j] = little_len + 1;
    pattern = (unsigned char *) little;
    for (j = 0; j < little_len; ++j)
        bad[pattern[j]] = little_len - j;
    q = (unsigned char *) big;
    q_stop = (unsigned char *) big + big_len - little_len;
    while (q <= q_stop) {
        q_check = q;
        pattern = (unsigned char *) little;
        to_match = little_len;
        /* Compare pattern to text */
        while (to_match && *q_check++ == *pattern++)
            --to_match;
        /* Match found */
        if (!to_match)
            return (char *) q;
        /* Jump using the bad character table */
        q += bad[q[little_len]];
    }
    /* Not found */
    return NULL;
}

int sane_standard_streams(void)
{
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1)
        return 1;
    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        return 1;
    if (_setmode(_fileno(stderr), _O_BINARY) == -1)
        return 1;
#endif
    return 0;
}

/* ************************************************************************** */

int addnstr(char *str, int n)
{
    char ch;
    int ret;
    while (n-- && (ch = *str++)) {
        printch(ch);
        if (ret)
            return 1;
    }
    return 0;
}

int get_screen_size(size_t * height, size_t * width)
{
    /* Gets the screen size */
#ifdef _WIN32
    HANDLE out;
    CONSOLE_SCREEN_BUFFER_INFO info;
    if ((out = GetStdHandle(STD_OUTPUT_HANDLE)) == INVALID_HANDLE_VALUE)
        return 1;
    if (!GetConsoleScreenBufferInfo(out, &info))
        return 1;
    *height = info.srWindow.Bottom - info.srWindow.Top + 1;
    *width = info.srWindow.Right - info.srWindow.Left + 1;
    return 0;
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
        return 1;
    *height = ws.ws_row;
    *width = ws.ws_col;
    return 0;
#endif
}

int erase(void)
{
    size_t new_h, new_w, req_vms;
    char *tmp_ns, *tmp_cs;
    if (get_screen_size(&new_h, &new_w))
        return 1;

    /* Reset virtual index */
    stdscr->v = 0;

    /* Clear hard or change in screen dimensions */
    if (stdscr->hard || new_h != stdscr->h || new_w != stdscr->w) {
        stdscr->h = new_h;
        stdscr->w = new_w;
        if (mof(stdscr->h, stdscr->w))
            return 1;
        stdscr->sa = stdscr->h * stdscr->w;
        /*
         * Add TABSIZE to the end of the virtual screen to
         * allow for characters to be printed off the screen.
         * Assumes that tab consumes the most screen space
         * out of all the characters.
         */
        if (aof(stdscr->sa, TABSIZE))
            return 1;
        req_vms = stdscr->sa + TABSIZE;
        /* Bigger screen */
        if (stdscr->vms < req_vms) {
            if ((tmp_ns = malloc(req_vms)) == NULL)
                return 1;
            if ((tmp_cs = malloc(req_vms)) == NULL) {
                free(tmp_ns);
                return 1;
            }
            free(stdscr->ns);
            stdscr->ns = tmp_ns;
            free(stdscr->cs);
            stdscr->cs = tmp_cs;
            stdscr->vms = req_vms;
        }
        /*
         * Clear the virtual current screen. No need to erase the
         * virtual screen beyond the physical screen size.
         */
        memset(stdscr->cs, ' ', stdscr->sa);
        phy_attr_off();
        stdscr->phy_iv = 0;
        phy_clear_screen();
        stdscr->hard = 0;
    }
    /* Clear the virtual next screen */
    memset(stdscr->ns, ' ', stdscr->sa);
    return 0;
}

int clear(void)
{
    stdscr->hard = 1;
    return erase();
}

int end_scr(void)
{
    int ret = 0;
    phy_attr_off();
    phy_clear_screen();
#ifdef _WIN32
    if (!SetConsoleMode(stdscr->h_out, stdscr->t_orig))
        ret = 1;
#else
    if (stdscr->tty && tcsetattr(STDIN_FILENO, TCSANOW, &stdscr->t_orig))
        ret = 1;
#endif
    free(stdscr->ns);
    free(stdscr->cs);
    free_buf(stdscr->input);
    free(stdscr);
    return ret;
}

int init_scr(void)
{
#ifdef _WIN32
    HANDLE h_out;
    DWORD term_orig;
#else
    struct termios term_orig, term_new;
#endif
    if ((stdscr = calloc(1, sizeof(struct graph))) == NULL)
        return 1;
    if ((stdscr->input = init_buf(INIT_BUF_SIZE)) == NULL) {
        free(stdscr);
        return 1;
    }
#ifdef _WIN32
    /* Check input is from a terminal */
    if (_isatty(_fileno(stdin)))
        stdscr->tty = 1;

    /* Turn on interpretation of VT100-like escape sequences */
    if ((h_out = GetStdHandle(STD_OUTPUT_HANDLE)) == INVALID_HANDLE_VALUE) {
        free_buf(stdscr->input);
        free(stdscr);
        stdscr = NULL;
        return 1;
    }
    if (!GetConsoleMode(h_out, &term_orig)) {
        free_buf(stdscr->input);
        free(stdscr);
        stdscr = NULL;
        return 1;
    }
    if (!SetConsoleMode
        (h_out, term_orig | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        free_buf(stdscr->input);
        free(stdscr);
        stdscr = NULL;
        return 1;
    }
#else
    if (isatty(STDIN_FILENO)) stdscr->tty = 1;

if (stdscr->tty) {
    /* Change terminal input to raw and no echo */
    if (tcgetattr(STDIN_FILENO, &term_orig)) {
        free_buf(stdscr->input);
        free(stdscr);
        stdscr = NULL;
        return 1;
    }
    term_new = term_orig;
    cfmakeraw(&term_new);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &term_new)) {
        free_buf(stdscr->input);
        free(stdscr);
        stdscr = NULL;
        return 1;
    }
}
#endif

#ifdef _WIN32
    stdscr->h_out = h_out;
#endif

    stdscr->t_orig = term_orig;
    return 0;
}

void draw_diff(void)
{
    /* Physically draw the screen where the virtual screens differ */
    int in_pos = 0;             /* In position for printing */
    char ch;
    size_t i;
    for (i = 0; i < stdscr->sa; ++i) {
        if ((ch = *(stdscr->ns + i)) != *(stdscr->cs + i)) {
            if (!in_pos) {
                /* Top left corner is (1, 1) not (0, 0) so need to add one */
                phy_move_cursor(i / stdscr->w + 1, i % stdscr->w + 1);
                in_pos = 1;
            }
            /* Inverse video mode */
            if (ivon(ch) && !stdscr->phy_iv) {
                phy_inverse_video();
                stdscr->phy_iv = 1;
            } else if (!ivon(ch) && stdscr->phy_iv) {
                phy_attr_off();
                stdscr->phy_iv = 0;
            }
            putchar(ch & 0x7F);
        } else {
            in_pos = 0;
        }
    }
}

int refresh(void)
{
    char *t;
    draw_diff();
    /* Set physical cursor to the position of the virtual cursor */
    if (stdscr->v < stdscr->sa)
        phy_move_cursor(stdscr->v / stdscr->w + 1,
                        stdscr->v % stdscr->w + 1);
    else
        phy_move_cursor(stdscr->h, stdscr->w);
    /* Swap virtual screens */
    t = stdscr->cs;
    stdscr->cs = stdscr->ns;
    stdscr->ns = t;
    return 0;
}

int getch(void)
{
    /* Process multi-char keys */
#ifdef _WIN32
    int x;
    if ((x = getch_nk()) != 0xE0)
        return x;
    switch (x = getch_nk()) {
    case 'G':
        return KEY_HOME;
    case 'H':
        return KEY_UP;
    case 'K':
        return KEY_LEFT;
    case 'M':
        return KEY_RIGHT;
    case 'O':
        return KEY_END;
    case 'P':
        return KEY_DOWN;
    case 'S':
        return KEY_DC;
    default:
        if (ungetch(x))
            return EOF;
        return 0xE0;
    }
#else
    int x, z;
    if ((x = getch_nk()) != ESC)
        return x;
    if ((x = getch_nk()) != '[') {
        if (ungetch(x))
            return EOF;
        return ESC;
    }
    x = getch_nk();
    if (x != 'A' && x != 'B' && x != 'C' && x != 'D' && x != 'F'
        && x != 'H' && x != '1' && x != '3' && x != '4') {
        if (ungetch(x))
            return EOF;
        if (ungetch('['))
            return EOF;
        return ESC;
    }
    switch (x) {
    case 'A':
        return KEY_UP;
    case 'B':
        return KEY_DOWN;
    case 'C':
        return KEY_RIGHT;
    case 'D':
        return KEY_LEFT;
    }
    if ((z = getch_nk()) != '~') {
        if (ungetch(z))
            return EOF;
        if (ungetch(x))
            return EOF;
        if (ungetch('['))
            return EOF;
        return ESC;
    }
    switch (x) {
    case '1':
        return KEY_HOME;
    case '3':
        return KEY_DC;
    case '4':
        return KEY_END;
    }
    return EOF;
#endif
}

/* ************************************************************************** */

struct atom *compile_regex(char *find, struct cap_grp *cg, struct hook *hk)
{
    struct atom *cr;
    unsigned char u, w, e;
    size_t i, j, k, n, atom_index = 0;
    int in_set = 0;
    size_t cap_grp_index = 0;   /* Capture group index of open bracket */
    size_t stack[NUM_CAP_GRP] = { 0 };  /* Stack for nested capture groups */
    size_t si = 0;              /* Stack index */
    size_t len = strlen(find);

    /* Addition is OK as this size is already in memory */
    if ((cr = calloc(len + 1, sizeof(struct atom))) == NULL)
        return NULL;

    /* Set defaults */
    for (i = 0; i < len + 1; ++i) {
        cr[i].min_occ = 1;
        cr[i].max_occ = 1;
        cr[i].num = 1;
    }

    /* Clear the capture groups */
    for (j = 0; j < NUM_CAP_GRP; ++j) {
        cg[j].atom_start = -1;
        cg[j].atom_end = -1;
        cg[j].p = NULL;
        cg[j].len = 0;
    }

    /* Clear hooks */
    hk->start = '\0';
    hk->end = '\0';

    /* Check for ^ hook */
    if (*find == '^') {
        hk->start = 'Y';
        /* Eat char */
        ++find;
    }

    while ((u = *find++)) {
        switch (u) {
        case '\\':
            if (!*find)
                cr_bail();
            switch (*find) {
                /* Process special character sets first */
            case 'w':
                /* Word set */
                for (k = 0; k < ASCII_NUM; ++k)
                    if (isalnum(k) || k == '_')
                        cr[atom_index].set[k] = 'Y';
                break;
            case 'W':
                /* Non-word set */
                for (k = 0; k < ASCII_NUM; ++k)
                    if (!(isalnum(k) || k == '_'))
                        cr[atom_index].set[k] = 'Y';
                break;
            case 'd':
                /* Digit set */
                for (k = 0; k < ASCII_NUM; ++k)
                    if (isdigit(k))
                        cr[atom_index].set[k] = 'Y';
                break;
            case 'D':
                /* Non-digit set */
                for (k = 0; k < ASCII_NUM; ++k)
                    if (!isdigit(k))
                        cr[atom_index].set[k] = 'Y';
                break;
            case 's':
                /* Whitespace set */
                for (k = 0; k < ASCII_NUM; ++k)
                    if (isspace(k))
                        cr[atom_index].set[k] = 'Y';
                break;
            case 'S':
                /* Non-whitespace set */
                for (k = 0; k < ASCII_NUM; ++k)
                    if (!isspace(k))
                        cr[atom_index].set[k] = 'Y';
                break;
            default:
                /* Escaping, the next char is considered a literal */
                cr[atom_index].set[(unsigned char) *find] = 'Y';
                break;
            }
            if (!in_set)
                ++atom_index;
            /* Eat char */
            ++find;
            break;
        case '$':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else if (!*find) {
                /* Hook */
                hk->end = 'Y';
            } else {
                /* Just a stand-alone char */
                cr[atom_index].set[u] = 'Y';
                ++atom_index;
            }
            break;
        case '(':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else {
                ++cap_grp_index;
                if (cap_grp_index >= NUM_CAP_GRP)
                    cr_bail();
                cg[cap_grp_index].atom_start = atom_index;
                stack[si++] = cap_grp_index;
            }
            break;
        case ')':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else {
                if (!cap_grp_index || !si)
                    cr_bail();
                cg[stack[--si]].atom_end = atom_index;
            }
            break;
        case '[':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else {
                in_set = 1;
                /* Look ahead for negation operator */
                if (*find == '^') {
                    cr[atom_index].negate = 'Y';
                    /* Eat char */
                    ++find;
                }
            }
            break;
        case ']':
            in_set = 0;
            ++atom_index;
            break;
        case '{':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else {
                /* Range */
                if (!atom_index)
                    cr_bail();

                /* Read first number (if any) */
                n = 0;
                while (isdigit(w = *find++)) {
                    if (mof(n, 10))
                        cr_bail();
                    n *= 10;
                    if (aof(n, w - '0'))
                        cr_bail();
                    n += w - '0';
                }
                /* Zero is the default here if no first number was given */
                cr[atom_index - 1].min_occ = n;

                if (w == '}') {
                    /*
                     * Only one number or empty braces. Cannot be zero here.
                     * Set max_occ to the same number.
                     */
                    if (!n)
                        cr_bail();
                    cr[atom_index - 1].max_occ = n;
                    break;      /* Done reading range */
                } else if (w != ',') {
                    /* Syntax error */
                    cr_bail();
                }

                /* Read second number (if any) */
                /* Deafult is -1 here if no second number is given */
                cr[atom_index - 1].max_occ = -1;
                n = 0;
                while (isdigit(w = *find++)) {
                    if (mof(n, 10))
                        cr_bail();
                    n *= 10;
                    if (aof(n, w - '0'))
                        cr_bail();
                    n += w - '0';
                    /* Update number */
                    cr[atom_index - 1].max_occ = n;
                }
                /* The second number cannot be zero */
                if (!cr[atom_index - 1].max_occ)
                    cr_bail();

                /* End range */
                if (w != '}')
                    cr_bail();
            }
            break;
        case '*':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else {
                if (!atom_index)
                    cr_bail();
                cr[atom_index - 1].min_occ = 0;
                cr[atom_index - 1].max_occ = -1;
            }
            break;
        case '+':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else {
                if (!atom_index)
                    cr_bail();
                cr[atom_index - 1].min_occ = 1;
                cr[atom_index - 1].max_occ = -1;
            }
            break;
        case '?':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else {
                if (!atom_index)
                    cr_bail();
                cr[atom_index - 1].min_occ = 0;
                cr[atom_index - 1].max_occ = 1;
            }
            break;
        case '.':
            if (in_set) {
                cr[atom_index].set[u] = 'Y';
            } else {
                /* All possible chars */
                for (k = 0; k < UCHAR_NUM; ++k)
                    cr[atom_index].set[k] = 'Y';
                ++atom_index;
            }
            break;
        default:
            if (in_set) {
                /* Look ahead for range */
                if (*find == '-' && (e = *(find + 1))) {
                    if (e < u)
                        cr_bail();
                    for (i = u; i <= e; ++i)
                        cr[atom_index].set[i] = 'Y';
                    /* Eat chars */
                    find += 2;
                } else {
                    /* Just add the char to the set */
                    cr[atom_index].set[u] = 'Y';
                }
            } else {
                cr[atom_index].set[u] = 'Y';
                ++atom_index;
            }
            break;
        }
    }

    /* Terminate atom array */
    cr[atom_index].end = 'Y';

    /* Checks: */
    /* Unclosed character group */
    if (in_set)
        cr_bail();
    /* Unclosed capture group */
    for (j = 0; j < NUM_CAP_GRP; ++j)
        if (cg[j].atom_start != -1 && cg[j].atom_end == -1)
            cr_bail();

    /* Set capture group zero */
    cg[0].atom_start = 0;
    cg[0].atom_end = atom_index;

    return cr;
}

/* Function declaration is required because of the recursion */
char *match_regex_here(struct atom *find, struct hook *hk, char *str);
char *match_regex_mult(struct atom *find, struct hook *hk, char *str);

char *match_regex(struct atom *find, struct hook *hk, char *str,
                  int sol, size_t * len)
{
    char *end_p;

    /*
     * Short-circuit if there is a start of line hook ^ and the string does not
     * begin at the start of the line.
     */
    if (hk->start && !sol)
        return NULL;

    /* End of line hook $ by itself */
    if (hk->end && find->end) {
        *len = 0;
        return str + strlen(str);
    }

    do {
        end_p = match_regex_here(find, hk, str);
        /* Keep searching if there is no start hook ^ and no match */
    } while (!hk->start && end_p == NULL && *str++ != '\0');

    if (end_p == NULL)
        return NULL;

    *len = end_p - str;         /* Length of match */
    return str;
}

char *match_regex_here(struct atom *find, struct hook *hk, char *str)
{
    /* End of regex atom chain */
    if (find->end) {
        /* Failed end hook */
        if (hk->end && *str)
            return NULL;
        /* Match */
        return str;
    }
    if (find->min_occ == 1 && find->max_occ == 1
        && *str != '\0' && match_atom(find, *str))
        return match_regex_here(++find, hk, ++str);
    if (!(find->min_occ == 1 && find->max_occ == 1))
        return match_regex_mult(find, hk, str);

    /* No match */
    return NULL;
}

char *match_regex_mult(struct atom *find, struct hook *hk, char *str)
{
    char *t = str, *r;
    /* Find the most repeats possible */
    while (*t != '\0' && match_atom(find, *t)
           && (find->max_occ == -1 ? 1 : t - str < find->max_occ))
        ++t;
    /* Too few matches */
    if (t - str < find->min_occ)
        return NULL;
    /* Work backwards to see if the rest of the pattern will match */
    while (t - str >= find->min_occ) {
        r = match_regex_here(find + 1, hk, t);
        if (r != NULL) {
            /* Record the number of times that this atom was matched */
            find->num = t - str;
            return r;
        }
        --t;
    }
    /* No match */
    return NULL;
}

void fill_in_capture_groups(struct cap_grp *cg, char *match_p,
                            struct atom *find)
{
    ssize_t i = 0, j, running_total = 0;
    /* Clear values */
    for (j = 0; j < NUM_CAP_GRP; ++j) {
        cg[j].p = NULL;
        cg[j].len = 0;
    }

    while (!find[i].end) {
        for (j = 0; j < NUM_CAP_GRP; ++j) {
            if (i == cg[j].atom_start)
                cg[j].p = match_p + running_total;
            if (i >= cg[j].atom_start && i < cg[j].atom_end)
                cg[j].len += find[i].num;
        }
        running_total += find[i].num;
        ++i;
    }

    /* Set length on capture group zero */
    cg[0].len = running_total;
}

char *regex_replace(char *str, char *find, char *replace, int nl_insen)
{
    /*
     * Regular expression find and replace. Returns NULL upon failure.
     * Must free the returned string after success.
     */
    int ret = 0;
    char *t = NULL, *line, *q = NULL;
    char *p;                    /* Pointer to regex match */
    size_t len = 0;             /* Length of regex match */
    size_t prev_len = 0;        /* Length of previous regex match */
    char *rep, ch;
    size_t j;
    struct buf *result = NULL;
    size_t init_buf_size = BUFSIZ;
    char *res_str = NULL;
    struct atom *cr = NULL;
    struct cap_grp cg[NUM_CAP_GRP];
    struct hook hk;
    int sol = 1;                /* Start of line indicator */

    /* Compile regex expression */
    if ((cr = compile_regex(find, cg, &hk)) == NULL)
        return NULL;

    /*
     * Copy string if in newline sensitive mode as the \n chars will be
     * replaced with \0.
     */
    if (!nl_insen) {
        if ((t = xstrdup(str)) == NULL)
            quit();
        else
            str = t;
    }

    if ((result = init_buf(init_buf_size)) == NULL)
        quit();

    /*
     * Do not process an empty string (but an in-the-middle line can be empty).
     * Not an error.
     */
    if (!*str)
        goto clean_up;

    line = str;

    /* Process line by line */
    do {
        /* Terminate line */
        if (!nl_insen) {
            if ((q = strchr(line, '\n')) != NULL)
                *q = '\0';
            /* Do not process the last line if empty. Not an error. */
            else if (!*line)
                goto clean_up;
        }

        while (1) {
            /* Match regex as many times as possible on line */
            if ((p = match_regex(cr, &hk, line, sol, &len)) == NULL)
                break;

            /* Turn off as there can be multiple matches on the same line */
            sol = 0;
            /* Fill in caputure group matches */
            fill_in_capture_groups(cg, p, cr);

            /* Copy text before the match  */
            if (put_mem(result, line, p - line))
                quit();

            /*
             * Skip putting the replacement text if a zero length match and the
             * previous match was not a zero length match. This stops two
             * replacements being put at the same location. Only has scope
             * within a line.
             */
            if (!(!len && prev_len && p == line)) {
                /* Copy replacement text */
                rep = replace;
                while ((ch = *rep++)) {
                    if (ch == '\\' && isdigit(*rep)) {
                        /* Substitute backreferences to capture groups */
                        j = *rep - '0';
                        ++rep;  /* Eat the digit */
                        /*
                         * Backreference exceeds number of capture groups in the
                         * orginal find regex.
                         */
                        if (cg[j].atom_start == -1)
                            quit();
                        if (cg[j].len
                            && put_mem(result, cg[j].p, cg[j].len))
                            quit();
                    } else {
                        if (put_ch(result, ch))
                            quit();
                    }
                }
            }

            /* Record prev len for the next iteration */
            prev_len = len;

            /* Move forward on the same line to after the end of the match */
            line = p + len;
            /* Break if at end of line */
            if (!*line)
                break;
            /*
             * Move forward by one if a zero length match, and pass the skipped
             * character through.
             */
            if (!len && put_ch(result, *line++))
                quit();
        }

        /* Copy the rest of the line */
        if (put_str(result, line))
            quit();
        /* Replace the newline character */
        if (!nl_insen && q != NULL && put_ch(result, '\n'))
            quit();

        /* Reset start of line indicator ready for the next line */
        sol = 1;
        /* Clear previous match len as this does not carry from one line to the next */
        prev_len = 0;
        /* Move to the next line if doing newline sensitive matching */
        if (!nl_insen && q != NULL)
            line = q + 1;
    } while (!nl_insen && q != NULL);

  clean_up:
    free(cr);
    free(t);

    /* Terminate buf in case it is used as a string */
    if (!ret && put_ch(result, '\0'))
        ret = 1;
    /* Free buffer on failure */
    if (ret) {
        free_buf(result);
        return NULL;
    }

    if (result != NULL)
        res_str = result->a;
    free_buf_wrapping(result);

    return res_str;
}

/* ************************************************************************** */

int grow_gap(struct gapbuf *b, size_t will_use)
{
    /* Grows the gap size of a gap buffer */
    char *t, *new_e;
    size_t gapbuf_size = b->e - b->a + 1;
    if (mof(gapbuf_size, 2))
        return 1;
    gapbuf_size *= 2;
    if (aof(gapbuf_size, will_use))
        return 1;
    gapbuf_size += will_use;
    if ((t = malloc(gapbuf_size)) == NULL)
        return 1;
    /* Copy text before the gap */
    memcpy(t, b->a, b->g - b->a);
    /* Copy text after the gap, excluding the end of gap buffer character */
    new_e = t + gapbuf_size - 1;
    memcpy(new_e - (b->e - b->c), b->c, b->e - b->c);
    /* Set end of gap buffer character */
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

struct gapbuf *init_gapbuf(size_t init_gapbuf_size)
{
    /* Initialises a gap buffer */
    struct gapbuf *b;
    if ((b = malloc(sizeof(struct gapbuf))) == NULL)
        return NULL;
    if ((b->a = malloc(init_gapbuf_size)) == NULL)
        return NULL;
    b->e = b->a + init_gapbuf_size - 1;
    b->g = b->a;
    b->c = b->e;
    /* End of gap buffer char. Cannot be deleted. */
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

void free_gapbuf_list(struct gapbuf *b)
{
    /* Frees a gap buffer doubly linked list (can be a single gap buffer) */
    struct gapbuf *t = b, *next;
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

int insert_ch(struct gapbuf *b, char c, size_t mult)
{
    /* Inserts a char mult times into the gap buffer */
    if (!mult)
        return 0;
    if (GAPSIZE(b) < mult)
        if (grow_gap(b, mult))
            return 1;
    while (mult--)
        INSERTCH(b, c);
    SETMOD(b);
    return 0;
}

int delete_ch(struct gapbuf *b, size_t mult)
{
    /* Deletes mult chars in a gap buffer */
    if (!mult)
        return 0;
    if (mult > (size_t) (b->e - b->c))
        return 1;
    while (mult--)
        DELETECH(b);
    SETMOD(b);
    return 0;
}

int backspace_ch(struct gapbuf *b, size_t mult)
{
    /* Backspaces mult chars in a gap buffer */
    if (!mult)
        return 0;
    if (mult > CURSOR_INDEX(b))
        return 1;
    while (mult--)
        BACKSPACECH(b);
    SETMOD(b);
    return 0;
}

int left_ch(struct gapbuf *b, size_t mult)
{
    /* Move the cursor left mult positions */
    if (!mult)
        return 0;
    if (mult > CURSOR_INDEX(b))
        return 1;
    while (mult--)
        LEFTCH(b);
    return 0;
}

int right_ch(struct gapbuf *b, size_t mult)
{
    /* Move the cursor right mult positions */
    if (!mult)
        return 0;
    if (mult > (size_t) (b->e - b->c))
        return 1;
    while (mult--)
        RIGHTCH(b);
    return 0;
}

void start_of_gapbuf(struct gapbuf *b)
{
    while (b->a != b->g)
        LEFTCH(b);
}

void end_of_gapbuf(struct gapbuf *b)
{
    while (b->c != b->e)
        RIGHTCH(b);
}

void start_of_line(struct gapbuf *b)
{
    while (b->a != b->g && *(b->g - 1) != '\n')
        LEFTCH(b);
}

void end_of_line(struct gapbuf *b)
{
    while (b->c != b->e && *b->c != '\n')
        RIGHTCH(b);
}

size_t col_num(struct gapbuf *b)
{
    /* Returns the column number, which starts from zero */
    char *q = b->g;
    while (q != b->a && *(q - 1) != '\n')
        --q;
    return b->g - q;
}

int up_line(struct gapbuf *b, size_t mult)
{
    /* Moves the cursor up mult lines */
    size_t col;
    size_t target_row;
    if (!mult)
        return 0;
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

int down_line(struct gapbuf *b, size_t mult)
{
    /* Moves the cursor down mult lines */
    size_t col;
    size_t target_row;
    char *c_backup = b->c;

    if (!mult)
        return 0;

    if (aof(b->r, mult))
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

void forward_word(struct gapbuf *b, int mode, size_t mult)
{
    /*
     * Moves forward up to mult words. If mode is 0 then no editing
     * occurs. If mode is 1 then words will be converted to uppercase,
     * and if mode is 3 then words will be converted to lowercase.
     */
    int mod = 0;
    if (!mult)
        return;

    while (b->c != b->e && mult--) {
        /* Eat leading non-alphanumeric characters */
        while (b->c != b->e && is_ascii(*b->c) && !isalnum(*b->c))
            RIGHTCH(b);

        /* Convert letters to uppercase while in the alphanumeric word */
        while (b->c != b->e && is_ascii(*b->c) && isalnum(*b->c)) {
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

void backward_word(struct gapbuf *b, size_t mult)
{
    /* Moves back a maximum of mult words */
    if (!mult)
        return;
    while (b->g != b->a && mult--) {
        /* Eat trailing non-alphanumeric characters */
        while (b->g != b->a && is_ascii(*(b->g - 1))
               && !isalnum(*(b->g - 1)))
            LEFTCH(b);
        /* Go to start of word */
        while (b->g != b->a && is_ascii(*(b->g - 1))
               && isalnum(*(b->g - 1))) {
            LEFTCH(b);
        }
    }
}

void trim_clean(struct gapbuf *b)
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

    end_of_gapbuf(b);

    /* Delete to end of text, sparing the first newline character */
    while (b->g != b->a) {
        LEFTCH(b);
        if (is_ascii(*b->c) && isgraph(*b->c)) {
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
        } else if (is_ascii(*b->c) && isgraph(*b->c)) {
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

void str_gapbuf(struct gapbuf *b)
{
    /* Prepares a gap buffer so that b->c can be used as a string */
    end_of_gapbuf(b);
    while (b->a != b->g) {
        LEFTCH(b);
        if (*b->c == '\0')
            DELETECH(b);
    }
}

void set_mark(struct gapbuf *b)
{
    b->m = CURSOR_INDEX(b);
    b->mr = b->r;
    b->m_set = 1;
}

void clear_mark(struct gapbuf *b)
{
    b->m = 0;
    b->mr = 1;
    b->m_set = 0;
}

int forward_search(struct gapbuf *b, char *p, size_t n)
{
    /* Forward search gap buffer b for memory p (n chars long) */
    char *q;
    if (b->c == b->e)
        return 1;
    if ((q = memmatch(b->c + 1, b->e - b->c - 1, p, n)) == NULL)
        return 1;
    while (b->c != q)
        RIGHTCH(b);
    return 0;
}

char *regex_search(char *str, char *find, int nl_insen, int *err)
{
    /*
     * Regular expression search. Returns a pointer to the first match
     * of find in str, or NULL upon no match or error (and sets err
     * to 1 upon error).
     */
    char *t = NULL, *text, *line, *q = NULL;
    char *p;                    /* Pointer to regex match */
    size_t len = 0;             /* Length of regex match */
    struct atom *cr = NULL;
    struct cap_grp cg[NUM_CAP_GRP];
    struct hook hk;

    /* Compile regex expression */
    if ((cr = compile_regex(find, cg, &hk)) == NULL) {
        *err = 1;
        return NULL;
    }

    /*
     * Copy string if in newline sensitive mode as the \n chars will be
     * replaced with \0.
     */
    if (!nl_insen) {
        if ((t = xstrdup(str)) == NULL) {
            free(cr);
            *err = 1;
            return NULL;
        }
        text = t;
    } else {
        text = str;
    }

    /*
     * Do not process an empty string (but an in-the-middle line can be empty).
     * Not an error.
     */
    if (!*text)
        goto no_match;

    line = text;

    /* Process line by line */
    do {
        /* Terminate line */
        if (!nl_insen) {
            if ((q = strchr(line, '\n')) != NULL)
                *q = '\0';
            /* Do not process the last line if empty. Not an error. */
            else if (!*line)
                goto no_match;
        }

        /* See if there is any match on a line */
        if ((p = match_regex(cr, &hk, line, 1, &len)) != NULL) {
            free(cr);
            free(t);
            /* Make the location relative to the original string */
            return str + (p - text);
        }

        /* Move to the next line if doing newline sensitive matching */
        if (!nl_insen && q != NULL)
            line = q + 1;
    } while (!nl_insen && q != NULL);

  no_match:
    free(cr);
    free(t);
    return NULL;
}

int regex_forward_search(struct gapbuf *b, char *find, int nl_insen)
{
    /*
     * Forward search gap buffer b for regex find string.
     * Will stop at the first embedded \0 char or at the End of Buffer \0 char.
     * Embedded \0 chars can be stripped by calling trim_clean first.
     */
    int err = 0;
    char *q;
    if (b->c == b->e)
        return 1;
    if ((q = regex_search(b->c + 1, find, nl_insen, &err)) == NULL)
        return 1;               /* No match or error */
    while (b->c != q)
        RIGHTCH(b);
    return 0;
}

void switch_cursor_and_mark(struct gapbuf *b)
{
    size_t orig_ci = CURSOR_INDEX(b);

    if (!b->m_set || b->m == orig_ci)
        return;

    if (b->m < orig_ci)
        while (CURSOR_INDEX(b) != b->m)
            LEFTCH(b);
    else
        while (CURSOR_INDEX(b) != b->m)
            RIGHTCH(b);

    b->m = orig_ci;
}

int delete_region(struct gapbuf *b)
{
    /* Deletes the region */
    /* Region does not exist, return error */
    if (!b->m_set)
        return 1;
    /* Region is empty, but no error */
    if (b->m == CURSOR_INDEX(b)) {
        /* Turn off region, no need to set modified indicator */
        b->m_set = 0;
        return 0;
    }
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

char *region_to_str(struct gapbuf *b)
{
    /*
     * Copies a region to a dynamically allocated string, stripping out
     * any \0 chars.
     */
    size_t ci, rs;
    char *str, *p, *q, ch;
    if (!b->m_set)
        return NULL;
    ci = CURSOR_INDEX(b);
    rs = b->m < ci ? ci - b->m : b->m - ci;
    /* Addition is OK here */
    if ((str = malloc(rs + 1)) == NULL)
        return NULL;
    p = str;
    q = b->m < ci ? INDEX_TO_POINTER(b, m) : b->c;
    while (rs--) {
        ch = *q++;
        if (ch != '\0')
            *p++ = ch;
    }
    *p = '\0';
    return str;
}

int regex_replace_region(struct gapbuf *b, char *dfdr, int nl_insen)
{
    /*
     * Regular expression replace region. dfdr is the regex find and replace
     * pattern which is of the form: delimiter find delimiter replace,
     * without the spaces, for example:
     * |cool|wow
     */
    size_t ci;                  /* Cursor index */
    size_t rs;                  /* Region size */
    char *res;                  /* Regex result string */
    size_t res_len;             /* Length of regex result string */
    char delim, *p, *find, *replace, *str;

    if (!b->m_set || dfdr == NULL || *dfdr == '\0')
        return 1;

    /* Split the input string in place into the find and replace components */
    delim = *dfdr;
    if ((p = strchr(dfdr + 1, delim)) == NULL)
        return 1;
    *p = '\0';
    find = dfdr + 1;
    replace = p + 1;

    ci = CURSOR_INDEX(b);
    rs = b->m < ci ? ci - b->m : b->m - ci;
    /* Nothing to do, but not an error */
    if (!rs)
        return 0;

    if ((str = region_to_str(b)) == NULL)
        return 1;

    if ((res = regex_replace(str, find, replace, nl_insen)) == NULL) {
        free(str);
        return 1;
    }

    free(str);

    res_len = strlen(res);

    /* Check space, as region will be deleted before the insert */
    if (res_len > rs && GAPSIZE(b) < res_len - rs
        && grow_gap(b, res_len - rs)) {
        free(res);
        return 1;
    }

    /* Delete region */
    if (delete_region(b)) {
        free(res);
        return 1;
    }

    /*
     * Right of gap insert.
     * Do not copy the terminating \0 char.
     */
    memcpy(b->c - (res_len - 1), res, res_len - 1);
    b->c -= res_len - 1;
    free(res);
    SETMOD(b);
    return 0;
}

int match_bracket(struct gapbuf *b)
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

int copy_region(struct gapbuf *b, struct gapbuf *p)
{
    /*
     * Copies the region from gap buffer b into gap buffer p.
     * The cursor is moved to the end of gap buffer p first.
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
    end_of_gapbuf(p);
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
    SETMOD(p);
    return 0;
}

int cut_region(struct gapbuf *b, struct gapbuf *p)
{
    /*
     * Copies the region from gap buffer b into gap buffer p.
     * The cursor is moved to the end of gap buffer p first.
     */
    if (copy_region(b, p))
        return 1;
    if (delete_region(b))
        return 1;
    return 0;
}

int paste(struct gapbuf *b, struct gapbuf *p, size_t mult)
{
    /*
     * Pastes (inserts) gap buffer p into gap buffer b mult times.
     * Moves the cursor to the end of gap buffer p first.
     */
    size_t num = mult;
    size_t s, ts;
    char *q;
    if (!mult)
        return 0;
    end_of_gapbuf(p);
    s = p->g - p->a;
    if (!s)
        return 0;
    if (mof(s, mult))
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

int cut_to_eol(struct gapbuf *b, struct gapbuf *p)
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

int cut_to_sol(struct gapbuf *b, struct gapbuf *p)
{
    /* Cut to the start of the line */
    set_mark(b);
    start_of_line(b);
    if (cut_region(b, p))
        return 1;
    return 0;
}

int insert_file(struct gapbuf *b, char *fn)
{
    /* Inserts a file into the righthand side of the gap */
    size_t fs;
    FILE *fp;
    int r;

    /* 2 if file does not exist or 1 on other errors */
    if ((r = filesize(fn, &fs)))
        return r;

    /* Nothing to do */
    if (!fs)
        return 0;
    if (fs > GAPSIZE(b))
        if (grow_gap(b, fs))
            return 1;
    if ((fp = fopen(fn, "rb")) == NULL)
        return 1;
    /* Right of gap insert */
    if (fread(b->c - fs, 1, fs, fp) != fs) {
        fclose(fp);
        return 1;
    }
    if (fclose(fp))
        return 1;

    b->c -= fs;
    SETMOD(b);
    return 0;
}

int write_file(struct gapbuf *b)
{
    /* Writes a file. Not safe, but ANSI C */
    FILE *fp;
    size_t n;

    if ((fp = fopen(b->fn, "wb")) == NULL)
        return 1;

    /* Before gap */
    n = b->g - b->a;
    if (fwrite(b->a, 1, n, fp) != n) {
        fclose(fp);
        return 1;
    }

    /* After gap, excluding the last character */
    n = b->e - b->c;
    if (fwrite(b->c, 1, n, fp) != n) {
        fclose(fp);
        return 1;
    }

    if (fclose(fp))
        return 1;

    /* Success */
    b->mod = 0;
    return 0;
}

/* ************************************************************************** */

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
    standend();

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
                int rv, int *req_centre, int *req_clear)
{
    /* Draws the text and command line gap buffers to the screen */
    int h, w;                   /* Screen size */
    int cl_cy, cl_cx;           /* Cursor position in command line */
    int cy = 0, cx = 0;         /* Final cursor position */
    int ret = 0;                /* Macro "return value" */
    char sb[200];               /* Status bar */
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

        /*
         * Cannot overrun buffer due to filename being caped to 80 chars.
         * 1 + 1 + 1 + 80 + 1 + 1 + 20 + 1 + 20 + 1 + 1 + 2 = 130
         * and buffer is 200.
         */
        if (sprintf
            (sb, "%c%c %.80s (%lu,%lu) %02X", rv ? '!' : ' ',
             b->mod ? '*' : ' ', b->fn,
             (unsigned long) (cl_active ? cl->r : b->r),
             (unsigned long) col_num(cl_active ? cl : b),
             (unsigned char) (cl_active ? *cl->c : *b->c)) < 0)
            return 1;
        if (addnstr(sb, w))
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
    if ((t = xstrdup(new_fn)) == NULL)
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
    int r;

    if ((t = init_gapbuf(INIT_GAPBUF_SIZE)) == NULL)
        return NULL;

    if (fn != NULL) {
        if ((r = insert_file(t, fn))) {
            if (r == 1) {
                /* Error */
                free_gapbuf_list(t);
                return NULL;
            }
        } else {
            /* No error */
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
    int req_centre = 0;         /* User requests cursor centreing */
    int req_clear = 0;          /* User requests screen clearing */
    int cl_active = 0;          /* Command line is being used */
    /* Operation for which command line is being used */
    char operation = ' ';
    size_t mult;                /* Command multiplier */
    /* Persist the sticky column (used for repeated up or down) */
    int persist_sc = 0;
    char last_search_type = ' ';
    int i;
    /* For displaying keybindings help */
    HELP;
    char **h;

    if (sane_standard_streams())
        quit();

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

    if (init_scr())
        quit();

    while (running) {
      top:
        if (draw_screen(b, cl, cl_active, rv, &req_centre, &req_clear))
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
                last_search_type = 's';
                start_of_gapbuf(cl);
                rv = forward_search(b, cl->c, cl->e - cl->c);
                break;
            case 'z':
                last_search_type = 'z';
                start_of_gapbuf(cl);
                rv = regex_forward_search(b, cl->c, 0);
                break;
            case 'Z':
                last_search_type = 'Z';
                start_of_gapbuf(cl);
                /* newline insensitive */
                rv = regex_forward_search(b, cl->c, 1);
                break;
            case 'r':
                str_gapbuf(cl);
                rv = regex_replace_region(b, cl->c, 0);
                break;
            case 'R':
                str_gapbuf(cl);
                /* newline insensitive */
                rv = regex_replace_region(b, cl->c, 1);
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
        case c('z'):
            /* Regex forward search, newline sensitive */
            DELETEGAPBUF(cl);
            operation = 'z';
            cl_active = 1;
            break;
        case c('r'):
            /* Regex replace region, newline sensitive */
            DELETEGAPBUF(cl);
            operation = 'r';
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
                /* Repeat the last search without editing the command line */
                if (!cl_active) {
                    start_of_gapbuf(cl);
                    switch (last_search_type) {
                    case 's':
                        rv = forward_search(z, cl->c, cl->e - cl->c);
                        break;
                    case 'z':
                        rv = regex_forward_search(z, cl->c, 0);
                        break;
                    case 'Z':  /* newline insensitive */
                        rv = regex_forward_search(z, cl->c, 1);
                        break;
                    }
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
                operation = 'R';
                cl_active = 1;
                break;
            case 'z':
                /* Regex forward search, newline insensitive */
                DELETEGAPBUF(cl);
                operation = 'Z';
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
    if (stdscr != NULL)
        if (end_scr())
            ret = 1;

    return ret;
}