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

/* toucan header file */

/* Sane feature test macros */

#ifndef TOUCAN_H
#define TOUCAN_H

#ifdef __linux__
/* For strdup, snprintf, popen */
#define _XOPEN_SOURCE 500
/* For readdir macros: DT_DIR and DT_REG */
#define _DEFAULT_SOURCE
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _CRT_RAND_S
#endif

#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <conio.h>
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <BaseTsd.h>
#else
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <libgen.h>
#include <dirent.h>
#include <unistd.h>
#endif

#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>


#ifdef _WIN32
#define popen _popen
#define pclose _pclose
/* For access */
#define F_OK 0
#endif


/* size_t Addition OverFlow test */
#define aof(a, b) ((a) > SIZE_MAX - (b))
/* size_t Multiplication OverFlow test */
#define mof(a, b) ((a) && (b) > SIZE_MAX / (a))

/* Minimum */
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

/* Maximum */
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

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

#define mquit(msg) do { \
    ret = 1; \
    fprintf(stderr, "Error: " msg "\n"); \
    goto clean_up; \
} while (0)

#ifdef _WIN32
#define ssize_t SSIZE_T
#endif


#ifdef _WIN32
/* For mkdir */
#include <direct.h>
#endif

#ifdef _WIN32
#define DIRSEP_CH '\\'
#define DIRSEP_STR "\\"
#else
#define DIRSEP_CH '/'
#define DIRSEP_STR "/"
#endif

/* Always want directories to have permissions of 0700 */
#ifndef _WIN32
#define mkdir(dn) mkdir(dn, S_IRWXU)
#endif


struct buf {
    char *a;
    size_t i;
    size_t s;
};

#define get_ch(b, read_stdin) (b->i ? *(b->a + --b->i) \
    : (read_stdin ? getchar() : EOF))

/*
 * put_ch is the same as unget_ch, just a different name depending on the
 * context. put_ch is used for output and unget_ch is used for input.
 */
#define put_ch unget_ch

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


/*
 * For hash table entries. Multpile entries at the same hash value link
 * together to form a singularly linked list.
 */
struct entry {
    struct entry *next;
    char *name;
    char *def;
};

struct hashtable {
    size_t n;                   /* Number of buckets */
    struct entry **b;           /* Buckets */
};


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


int str_to_num(char *s, size_t * num);
char *memmatch(char *big, size_t big_len, char *little, size_t little_len);
char *concat(char *str0, ...);
int sane_standard_streams(void);

char *random_alnum_str(size_t len);

int is_dir(char *dn);
int filesize(char *fn, size_t * fs);
char *file_to_str(char *fn);
int concat_path(char *res_path, char *dn, char *fn);
int walk_dir(char *dir_name, void *info,
             int (*process_file) (char *, void *));
int atomic_write(char *fn, void *info,
                 int (*write_details) (FILE *, void *));
int cp_file(char *from_file, char *to_file);
int exists(char *fn);
int read_pair_file(char *fn, void *info,
                   int (*process_pair) (char *, char *, void *));
int make_subdirs(char *file_path);
int create_new_file(char *fn);
int create_new_dir(char *dn);
char *make_tmp(char *in_dir, int dir);


int del_ch(struct buf *b);
struct buf *init_buf(size_t init_buf_size);
void free_buf_wrapping(struct buf *b);
void free_buf(struct buf *b);
int unget_ch(struct buf *b, int ch);
int include(struct buf *b, char *fn);
void delete_buf(struct buf *b);
int get_word(struct buf *token, struct buf *input, int read_stdin);
int unget_str(struct buf *b, char *str);
int put_str(struct buf *b, char *str);
int put_mem(struct buf *b, char *mem, size_t mem_s);
int buf_dump_buf(struct buf *dst, struct buf *src);
int write_buf(struct buf *b, char *fn);
int esyscmd(struct buf *input, struct buf *tmp_buf, char *cmd);


struct gapbuf *init_gapbuf(size_t init_gapbuf_size);
void free_gapbuf_list(struct gapbuf *b);
int insert_ch(struct gapbuf *b, char c, size_t mult);
int delete_ch(struct gapbuf *b, size_t mult);
int backspace_ch(struct gapbuf *b, size_t mult);
int left_ch(struct gapbuf *b, size_t mult);
int right_ch(struct gapbuf *b, size_t mult);
void start_of_gapbuf(struct gapbuf *b);
void end_of_gapbuf(struct gapbuf *b);
void start_of_line(struct gapbuf *b);
void end_of_line(struct gapbuf *b);
size_t col_num(struct gapbuf *b);
int up_line(struct gapbuf *b, size_t mult);
int down_line(struct gapbuf *b, size_t mult);
void forward_word(struct gapbuf *b, int mode, size_t mult);
void backward_word(struct gapbuf *b, size_t mult);
void trim_clean(struct gapbuf *b);
void str_gapbuf(struct gapbuf *b);
void set_mark(struct gapbuf *b);
void clear_mark(struct gapbuf *b);
int forward_search(struct gapbuf *b, char *p, size_t n);
int regex_forward_search(struct gapbuf *b, char *find, int nl_insen);
void switch_cursor_and_mark(struct gapbuf *b);
char *region_to_str(struct gapbuf *b);
int regex_replace_region(struct gapbuf *b, char *dfdr, int nl_insen);
int match_bracket(struct gapbuf *b);
int copy_region(struct gapbuf *b, struct gapbuf *p);
int delete_region(struct gapbuf *b);
int cut_region(struct gapbuf *b, struct gapbuf *p);
int insert_str(struct gapbuf *b, char *str, size_t mult);
int paste(struct gapbuf *b, struct gapbuf *p, size_t mult);
int cut_to_eol(struct gapbuf *b, struct gapbuf *p);
int cut_to_sol(struct gapbuf *b, struct gapbuf *p);
int insert_file(struct gapbuf *b, char *fn);
int write_file(struct gapbuf *b);


struct hashtable *init_hashtable(size_t num_buckets);
void free_hashtable(struct hashtable *ht);
void htdist(struct hashtable *ht);
struct entry *lookup_entry(struct hashtable *ht, char *name);
char *get_def(struct hashtable *ht, char *name);
int upsert_entry(struct hashtable *ht, char *name, char *def);
int delete_entry(struct hashtable *ht, char *name);
int write_hashtable(struct hashtable *ht, char *fn);
int load_file_into_ht(struct hashtable *ht, char *fn);


int addnstr(char *str, int n);
int erase(void);
int clear(void);
int endwin(void);
WINDOW *initscr(void);
int refresh(void);
int getch(void);


char *regex_replace(char *str, char *find, char *replace, int nl_insen);
char *regex_search(char *str, char *find, int nl_insen, int *err);


char *sha256(char *fn);

#endif
