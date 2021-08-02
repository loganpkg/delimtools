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

#ifdef __linux__
/* For strdup */
#define _XOPEN_SOURCE 500
#endif

#ifndef _WIN32
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../gen/gen.h"
#include "../buf/buf.h"
#include "../rx/rx.h"
#include "gapbuf.h"

/* Internal macros only */

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

static int grow_gap(struct gapbuf *b, size_t will_use)
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

int search(struct gapbuf *b, char *p, size_t n)
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

int regex_replace_region(struct gapbuf *b, char *dfdr, int nl_sen)
{
    /*
     * Regular expression replace region. dfdr is the regex find and replace
     * pattern which is of the form: delimiter find delimiter replace,
     * without the spaces, for example:
     * |cool|wow
     */
    size_t ci, rs;
    struct buf *res;
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

    if ((res = regex_replace(find, replace, str, nl_sen)) == NULL) {
        free(str);
        return 1;
    }

    free(str);

    /* Check space, as region will be deleted before the insert */
    if (res->i > rs && GAPSIZE(b) < res->i - rs
        && grow_gap(b, res->i - rs)) {
        free_buf(res);
        return 1;
    }

    /* Delete region */
    if (delete_region(b)) {
        free_buf(res);
        return 1;
    }

    /*
     * Right of gap insert.
     * Do not copy the terminating \0 char.
     */
    memcpy(b->c - (res->i - 1), res->a, res->i - 1);
    b->c -= res->i - 1;
    free_buf(res);
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

int insert_str(struct gapbuf *b, char *str, size_t mult)
{
    /* Inserts a string, str, into a gap buffer, b, mult times */
    size_t len;
    char *q;
    if (str == NULL)
        return 1;
    if (!mult || !(len = strlen(str)))
        return 0;
    if (mof(len, mult) || grow_gap(b, len * mult))
        return 1;
    q = b->c;
    while (mult--) {
        /* Insert after the cursor (right of gap) */
        q -= len;
        memcpy(q, str, len);
    }
    b->c = q;
    SETMOD(b);
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
    if (filesize(fn, &fs))
        return 1;
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
    /* Writes a gap buffer to file */
    int ret = 0;
    char *fn_tilde = NULL;
    FILE *fp = NULL;
    size_t n, fn_len;

#ifndef _WIN32
    char *fn_copy = NULL;
    char *dir;
    int fd;
    int d_fd = -1;
    struct stat st;
#endif

    /* No filename */
    if (b->fn == NULL || !(fn_len = strlen(b->fn)))
        quit();
    if (aof(fn_len, 2))
        quit();
    if ((fn_tilde = malloc(fn_len + 2)) == NULL)
        quit();
    memcpy(fn_tilde, b->fn, fn_len);
    *(fn_tilde + fn_len) = '~';
    *(fn_tilde + fn_len + 1) = '\0';
    if ((fp = fopen(fn_tilde, "wb")) == NULL)
        quit();

    /* Before gap */
    n = b->g - b->a;
    if (fwrite(b->a, 1, n, fp) != n)
        quit();

    /* After gap, excluding the last character */
    n = b->e - b->c;
    if (fwrite(b->c, 1, n, fp) != n)
        quit();

    if (fflush(fp))
        quit();

#ifndef _WIN32
    /* If original file exists, then apply its permissions to the new file */
    if (!stat(b->fn, &st) && S_ISREG(st.st_mode)
        && chmod(fn_tilde, st.st_mode & 0777))
        quit();
    if ((fd = fileno(fp)) == -1)
        quit();
    if (fsync(fd))
        quit();
#endif

    if (fclose(fp))
        quit();
    fp = NULL;

#ifndef _WIN32
    if ((fn_copy = strdup(b->fn)) == NULL)
        quit();
    if ((dir = dirname(fn_copy)) == NULL)
        quit();
    if ((d_fd = open(dir, O_RDONLY)) == -1)
        quit();
    if (fsync(d_fd))
        quit();
#endif

#ifdef _WIN32
    /* rename does not overwrite an existing file */
    errno = 0;
    if (remove(b->fn) && errno != ENOENT)
        quit();
#endif

    /* Atomic on POSIX systems */
    if (rename(fn_tilde, b->fn))
        quit();

#ifndef _WIN32
    if (fsync(d_fd))
        quit();
#endif

  clean_up:
    if (fn_tilde != NULL)
        free(fn_tilde);
    if (fp != NULL && fclose(fp))
        ret = 1;
#ifndef _WIN32
    if (fn_copy != NULL)
        free(fn_copy);
    if (d_fd != -1 && close(d_fd))
        ret = 1;
#endif

    /* Fail */
    if (ret)
        return 1;

    /* Success */
    b->mod = 0;
    return 0;
}
