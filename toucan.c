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

/* toucan C module */


/*
 * References:
 * Section 4.3 External Variables:
 * Brian W. Kernighan and Dennis M. Ritchie, The C Programming Language,
 *     Second Edition, Prentice Hall Software Series, New Jersey, 1988.
 * Clovis L. Tondo and Scott E. Gimpel, The C Answer Book, Second Edition,
 *     PTR Prentice Hall Software Series, New Jersey, 1989.
 *
 * Section 6.6 Table Lookup:
 * Brian W. Kernighan and Dennis M. Ritchie, The C Programming Language,
 *     Second Edition, Prentice Hall Software Series, New Jersey, 1988.
 * Clovis L. Tondo and Scott E. Gimpel, The C Answer Book, Second Edition,
 *     PTR Prentice Hall Software Series, New Jersey, 1989.
 *
 * Daniel J. Bernstein's djb2 algorithm from:
 * Hash Functions, http://www.cse.yorku.ca/~oz/hash.html
 *
 * 9.2 Regular Expressions:
 * Brian W. Kernighan and Rob Pike, The Practice of Programming,
 *     Addison Wesley Longman, Massachusetts, 1999.
 *
 * Brian Kernighan, Beautiful Code: Chapter 1: A Regular Expression Matcher,
 *     O'Reilly Media, California, 2007.
 *
 * FIPS PUB 180-4, Secure Hash Standard (SHS), National Institute of Standards
 *     and Technology, Maryland, August 2015.
 */


#include "toucan.h"

#define buf_free_size(b) (b->s - b->i)

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


#define INIT_BUF_SIZE 512

/* ANSI escape sequences */
#define phy_clear_screen() printf("\033[2J\033[1;1H")

/* Index starts at one. Top left is (1, 1) */
#define phy_move_cursor(y, x) printf("\033[%lu;%luH", (unsigned long) (y), \
    (unsigned long) (x))

#define phy_attr_off() printf("\033[m")

#define phy_inverse_video() printf("\033[7m")


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


#define bits_in_word 32
/* Rotate left */
#define ROTL(x, n) ((x >> n) | (x >> (bits_in_word - n)))
/* Rotate right */
#define ROTR(x, n) ((x >> n) | (x << (bits_in_word - n)))
/* Right shift */
#define SHR(x, n) (x >> n)

/* Functions */
#define Ch(x, y, z) ((x & y) ^ (~x & z))
#define Maj(x, y, z) ((x & y) ^ (x & z) ^ (y & z))
#define SIGMA0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define SIGMA1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define sigma0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ SHR(x, 3))
#define sigma1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ SHR(x, 10))


#define INIT_STR_BUF 512


static struct atom *compile_regex(char *find, struct cap_grp *cg,
                                  struct hook *hk);
static char *match_regex(struct atom *find, struct hook *hk, char *str,
                         int sol, size_t * len);
static char *match_regex_here(struct atom *find, struct hook *hk,
                              char *str);
static char *match_regex_mult(struct atom *find, struct hook *hk,
                              char *str);
static void fill_in_capture_groups(struct cap_grp *cg, char *match_p,
                                   struct atom *find);


int str_to_num(char *s, size_t * num)
{
    char ch;
    size_t n = 0, len = 0;
    if (s == NULL)
        return 1;
    while ((ch = *s++)) {
        ++len;
        if (isdigit(ch)) {
            if (mof(n, 10))
                return 1;
            n *= 10;
            if (aof(n, (ch - '0')))
                return 1;
            n += (ch - '0');
        } else {
            return 1;
        }
    }
    if (!len)
        return 1;
    *num = n;
    return 0;
}

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


char *concat(char *str0, ...)
{
/*
 * Concatenate multiple strings. Last argument must be NULL.
 * Returns NULL on error or a pointer to the concatenated string on success.
 * Must free the concatenated string after use.
 */
    int ret = 0;
    va_list arg_p;
    char *str;
    char ch, *p, *q, *tmp;
    size_t s, s_new, free_space, offset;

    if ((p = malloc(INIT_STR_BUF)) == NULL)
        return NULL;
    s = INIT_STR_BUF;
    free_space = s;

    va_start(arg_p, str0);
    str = str0;
    q = p;
    while (str != NULL) {
        while ((ch = *str++)) {
            /* Will use one with the terminating '\0' */
            if (free_space == 1) {
                /* Need to grow memory */
                if (mof(s, 2))
                    quit();
                s_new = s * 2;
                offset = q - p;
                if ((tmp = realloc(p, s_new)) == NULL)
                    quit();
                p = tmp;
                q = tmp + offset;
                free_space += s_new - s;
                s = s_new;
            }
            /* Copy char */
            *q++ = ch;
            --free_space;
        }
        str = va_arg(arg_p, char *);
    }
    *q = '\0';

  clean_up:
    va_end(arg_p);
    if (ret) {
        free(p);
        return NULL;
    }

    return p;
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


char *random_alnum_str(size_t len)
{
    /*
     * Returns a random alphanumeric string of length len.
     * Must free after use. Returns NULL upon failure.
     */
    char *p;
    unsigned char *t, u;
    size_t i, j;
#ifdef _WIN32
    unsigned int r;
    size_t ts = sizeof(unsigned int);
#else
    FILE *fp;
    size_t ts = 64;
#endif

    if (aof(len, 1))
        return NULL;
    if ((p = malloc(len + 1)) == NULL)
        return NULL;

#ifndef _WIN32
    if ((t = malloc(ts)) == NULL) {
        free(p);
        return NULL;
    }
    if ((fp = fopen("/dev/urandom", "r")) == NULL) {
        free(p);
        free(t);
        return NULL;
    }
#endif

    j = 0;
    while (j != len) {
#ifdef _WIN32
        if (rand_s(&r)) {
            free(p);
            return NULL;
        }
        t = (unsigned char *) &r;
#else
        if (fread(t, 1, ts, fp) != ts) {
            free(p);
            free(t);
            fclose(fp);
            return NULL;
        }
#endif

        for (i = 0; i < ts; ++i) {
            u = *(t + i);
            if (isalnum(u)) {
                *(p + j++) = u;
                if (j == len)
                    break;
            }
        }
    }

    *(p + j) = '\0';

#ifndef _WIN32
    free(t);
    if (fclose(fp)) {
        free(p);
        return NULL;
    }
#endif

    return p;
}

int is_dir(char *dn)
{
    /*
     * Returns 1 if dn is a directory.
     * Returns 0 if there was an error, or dn is not a directory.
     */
    struct stat st;
    if (stat(dn, &st))
        return 0;               /* Error */

#ifndef S_ISDIR
#define S_ISDIR(m) ((m & S_IFMT) == S_IFDIR)
#endif

    if (S_ISDIR(st.st_mode))
        return 1;               /* Yes. Is a directory. */
    else
        return 0;               /* No. Not a directory. */
}

int filesize(char *fn, size_t * fs)
{
    /* Gets the filesize of a filename */
    struct stat st;
    if (stat(fn, &st))
        return 1;

#ifndef S_ISREG
#define S_ISREG(m) ((m & S_IFMT) == S_IFREG)
#endif

    if (!S_ISREG(st.st_mode))
        return 1;
    if (st.st_size < 0)
        return 1;
    *fs = st.st_size;
    return 0;
}

char *file_to_str(char *fn)
{
    /* Reads a file fn to a dynamically allocated string */
    FILE *fp;
    size_t fs;
    char *str;
    if (filesize(fn, &fs))
        return NULL;
    if (aof(fs, 1))
        return NULL;
    if ((fp = fopen(fn, "rb")) == NULL)
        return NULL;
    if ((str = malloc(fs + 1)) == NULL) {
        fclose(fp);
        return NULL;
    }
    if (fread(str, 1, fs, fp) != fs) {
        fclose(fp);
        free(str);
        return NULL;
    }
    if (fclose(fp)) {
        free(str);
        return NULL;
    }
    *(str + fs) = '\0';
    return str;
}

#ifdef _WIN32
int walk_dir(char *dir_name, void *info,
             int (*process_file) (char *, void *))
{
    int ret = 0;
    WIN32_FIND_DATA wfd;
    HANDLE hdl;
    char *dn_wildcard = NULL;
    char *fn = NULL, *path_name = NULL;

    if ((dn_wildcard = concat(dir_name, DIRSEP_STR, "*", NULL)) == NULL)
        quit();
    if ((hdl = FindFirstFile(dn_wildcard, &wfd)) == INVALID_HANDLE_VALUE)
        quit();

    do {
        /* Copy filename */
        if ((fn = strdup(wfd.cFileName)) == NULL)
            quit();

        /* Concatenate file path */
        if ((path_name = concat(dir_name, DIRSEP_STR, fn, NULL)) == NULL)
            quit();

        if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (strcmp(fn, ".") && strcmp(fn, "..")
                && walk_dir(path_name, info, process_file))
                quit();
        } else {
            /* Assume it is a file */
            if ((*process_file) (path_name, info))
                quit();
        }

        free(dn_wildcard);
        dn_wildcard = NULL;
        free(fn);
        fn = NULL;
        free(path_name);
        path_name = NULL;

    } while (FindNextFile(hdl, &wfd));

    if (GetLastError() != ERROR_NO_MORE_FILES)
        ret = 1;

  clean_up:
    free(dn_wildcard);
    free(fn);
    free(path_name);

    if (!FindClose(hdl))
        ret = 1;

    return ret;
}
#else
int walk_dir(char *dir_name, void *info,
             int (*process_file) (char *, void *))
{
    int ret = 0;
    DIR *dirp = NULL;
    struct dirent *entry;
    unsigned char dt;
    char *fn = NULL, *path_name = NULL;

    if ((dirp = opendir(dir_name)) == NULL)
        quit();

    while ((entry = readdir(dirp)) != NULL) {
        /* Copy filename */
        if ((fn = strdup(entry->d_name)) == NULL)
            quit();
        dt = entry->d_type;

        /* Concatenate file path */
        if ((path_name = concat(dir_name, DIRSEP_STR, fn, NULL)) == NULL)
            quit();

        if (dt == DT_DIR) {
            if (strcmp(fn, ".") && strcmp(fn, "..")
                && walk_dir(path_name, info, process_file))
                quit();
        } else if (dt == DT_REG) {
            if ((*process_file) (path_name, info))
                quit();
        } else {
            /* Not a directory or regular (normal) file */
            quit();
        }

        free(fn);
        fn = NULL;
        free(path_name);
        path_name = NULL;
    }

  clean_up:
    free(fn);
    free(path_name);

    if (closedir(dirp))
        ret = 1;

    return ret;
}
#endif


int atomic_write(char *fn, void *info,
                 int (*write_details) (FILE *, void *))
{
    /* Performs atomic writes on POSIX systems */
    int ret = 0;
    char *fn_tilde = NULL;
    FILE *fp = NULL;
    size_t fn_len;

#ifndef _WIN32
    char *fn_copy = NULL;
    char *dir;
    int fd;
    int d_fd = -1;
    struct stat st;
#endif

    /* No filename */
    if (fn == NULL || !(fn_len = strlen(fn)))
        quit();
    if (aof(fn_len, 2))
        quit();
    if ((fn_tilde = malloc(fn_len + 2)) == NULL)
        quit();
    memcpy(fn_tilde, fn, fn_len);
    *(fn_tilde + fn_len) = '~';
    *(fn_tilde + fn_len + 1) = '\0';
    if ((fp = fopen(fn_tilde, "wb")) == NULL)
        quit();

    (*write_details) (fp, info);

    if (fflush(fp))
        quit();

#ifndef _WIN32
    /* If original file exists, then apply its permissions to the new file */
    if (!stat(fn, &st) && S_ISREG(st.st_mode)
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
    if ((fn_copy = strdup(fn)) == NULL)
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
    if (remove(fn) && errno != ENOENT)
        quit();
#endif

    /* Atomic on POSIX systems */
    if (rename(fn_tilde, fn))
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

    return 0;
}

int cp_file(char *from_file, char *to_file)
{
    int ret = 0;
    FILE *fp_from = NULL;
    FILE *fp_to = NULL;
    size_t fs, fs_left, io_size;
    char *p = NULL;

    if (filesize(from_file, &fs))
        return 1;

    if ((p = malloc(fs > BUFSIZ ? BUFSIZ : fs)) == NULL)
        return 1;

    if ((fp_from = fopen(from_file, "rb")) == NULL) {
        ret = 1;
        goto clean_up;
    }

    if ((fp_to = fopen(to_file, "wb")) == NULL) {
        ret = 1;
        goto clean_up;
    }

    fs_left = fs;

    while (fs_left) {
        io_size = min(BUFSIZ, fs_left);
        if (fread(p, 1, io_size, fp_from) != io_size) {
            ret = 1;
            goto clean_up;
        }
        if (fwrite(p, 1, io_size, fp_to) != io_size) {
            ret = 1;
            goto clean_up;
        }
        fs_left -= io_size;
    }

  clean_up:
    free(p);
    if (fp_from != NULL && fclose(fp_from))
        ret = 1;
    if (fp_to != NULL && fclose(fp_to))
        ret = 1;

    return ret;
}

int exists(char *fn)
{
    return !access(fn, F_OK);
}

int read_pair_file(char *fn, void *info,
                   int (*process_pair) (char *, char *, void *))
{
    /*
     * Read file with '\0' delimiter and entries in pairs.
     * Perform an operation on each pair {name, def}.
     */
    int ret = 0;
    FILE *fp;
    char *p = NULL, *q, *q_stop, *name, *def;
    size_t fs;
    if (filesize(fn, &fs))
        return 1;
    if (!fs)
        return 0;               /* Nothing to do */
    if ((fp = fopen(fn, "rb")) == NULL)
        quit();
    if ((p = malloc(fs)) == NULL)
        quit();
    if (fread(p, 1, fs, fp) != fs)
        quit();

    /* Test that data ends in '\0' */
    if (*(p + fs - 1) != '\0')
        quit();

    q = p;
    q_stop = p + fs - 1;

    while (1) {
        name = q;
        if ((q = memchr(q, '\0', q_stop - q + 1)) == q_stop)
            quit();
        ++q;
        def = q;
        if ((*process_pair) (name, def, info))
            quit();
        if ((q = memchr(q, '\0', q_stop - q + 1)) == q_stop)
            break;
        ++q;
    }

  clean_up:
    if (fp != NULL && fclose(fp))
        ret = 1;
    if (p != NULL)
        free(p);
    return ret;
}

int make_subdirs(char *file_path)
{
    /* Makes the subdirectories leading up to the file in file_path */
    char *p, *q;
    /* Copy the path so that it can be edited */
    if ((p = strdup(file_path)) == NULL)
        return 1;
    q = p;
    while ((q = strchr(q, DIRSEP_CH)) != NULL) {
        *q = '\0';
        if (strlen(p) && !is_dir(p) && mkdir(p)) {
            free(p);
            return 1;
        }
        *q = DIRSEP_CH;
        ++q;
    }
    free(p);
    return 0;
}

int create_new_file(char *fn)
{
    /*
     * Creates a new file fn.
     * Returns 0 on success.
     * Returns 2 if the file fn already exists (failure).
     * Otherwise returns 1 for all other failures.
     */
    int fd;
    errno = 0;
#ifdef _WIN32
    if (_sopen_s
        (&fd, fn, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _SH_DENYRW,
         _S_IREAD | _S_IWRITE))
#else
    if ((fd =
         open(fn, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR)) == -1)
#endif
    {
        if (errno == EEXIST)
            return 2;
        else
            return 1;
    }
#ifdef _WIN32
    if (_close(fd))
#else
    if (close(fd))
#endif
        return 1;
    return 0;
}

int create_new_dir(char *dn)
{
    /*
     * Creates a new directory dn.
     * Returns 0 on success.
     * Returns 2 if the directory dn already exists (failure).
     * Otherwise returns 1 for all other failures.
     */
    errno = 0;
    if (mkdir(dn)) {
        if (errno == EEXIST)
            return 2;
        else
            return 1;
    }
    return 0;
}

char *make_tmp(char *in_dir, int dir)
{
    /*
     * Creates a temporary directory (if dir is non-zero)
     * or file under in_dir.
     * Need to free after use. Returns NULL on failure.
     */
    char *name;
    char *path;
    char *d;
    size_t try = 10;            /* Number of times to try */
    int r;

    d = in_dir == NULL || *in_dir == '\0' ? "." : in_dir;

    while (try--) {
        if ((name = random_alnum_str(16)) == NULL)
            return NULL;
        if ((path = concat(d, DIRSEP_STR, name, NULL)) == NULL) {
            free(name);
            return NULL;
        }
        free(name);

        if (dir) {
            /* Create a temporary directory */
            if ((r = create_new_dir(path)) != 0) {
                free(path);
                if (r != 2)
                    return NULL;
            } else {
                /* Success */
                return path;
            }
        } else {
            /* Create a temporary file */
            if ((r = create_new_file(path)) != 0) {
                free(path);
                if (r != 2)
                    return NULL;
            } else {
                /* Success */
                return path;
            }
        }
    }
    /* All tries failed */
    return NULL;
}

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

static int grow_buf(struct buf *b, size_t will_use)
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

int del_ch(struct buf *b)
{
    if (b->i) {
        --b->i;
        return 0;
    }
    return 1;
}

int include(struct buf *b, char *fn)
{
    FILE *fp;
    int x;
    size_t fs;
    size_t back_i;

    if (filesize(fn, &fs))
        return 1;
    if (fs > buf_free_size(b) && grow_buf(b, fs))
        return 1;
    if ((fp = fopen(fn, "rb")) == NULL)
        return 1;
    back_i = b->i + fs;

    errno = 0;
    while ((x = getc(fp)) != EOF)
        *(b->a + --back_i) = x;

    if (errno) {
        fclose(fp);
        return 1;
    }
    if (fclose(fp))
        return 1;

    /* Check */
    if (back_i != b->i)
        return 1;

    /* Success */
    b->i += fs;

    return 0;
}

void delete_buf(struct buf *b)
{
    b->i = 0;
}

int get_word(struct buf *token, struct buf *input, int read_stdin)
{
    /*
     * Deletes the contents of the token buffer, then reads a word from the
     * input buffer and writes it to the token buffer. If read_stdin is
     * non-zero, then stdin will be read when the input buffer is empty.
     * This function discards \r chars.
     * Returns 1 on error and 2 on EOF (with no error).
     */
    int x;
    delete_buf(token);

    /* Always read at least one char */
    errno = 0;
    /* Discard \r chars */
    while ((x = get_ch(input, read_stdin)) == '\r');
    if (x == EOF) {
        if (errno)
            return 1;           /* Error */
        return 2;               /* EOF with no error */
    }

    if (put_ch(token, x))
        return 1;               /* Error */

    if (isalpha(x) || x == '_') {
        /* Could be the start of a macro name */
        while (1) {
            /* Read another char */
            errno = 0;
            /* Discard \r chars */
            while ((x = get_ch(input, read_stdin)) == '\r');
            if (x == EOF) {
                if (errno)
                    return 1;   /* Error */
                return 2;       /* EOF with no error */
            }
            if (!(isalnum(x) || x == '_')) {
                /* Read past the end of the token, so put the char back */
                if (unget_ch(input, x))
                    return 1;
                break;
            } else {
                /* Store the char */
                if (put_ch(token, x))
                    return 1;
            }
        }
    }
    /* Null terminate token */
    if (put_ch(token, '\0'))
        return 1;

    return 0;
}

int unget_str(struct buf *b, char *str)
{
    size_t len;
    if (str == NULL)
        return 1;
    if (!(len = strlen(str)))
        return 0;
    if (len > buf_free_size(b) && grow_buf(b, len))
        return 1;
    while (len)
        *(b->a + b->i++) = *(str + --len);
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

int buf_dump_buf(struct buf *dst, struct buf *src)
{
    if (src->i > buf_free_size(dst) && grow_buf(dst, src->i))
        return 1;
    memcpy(dst->a + dst->i, src->a, src->i);
    dst->i += src->i;
    src->i = 0;
    return 0;
}

int write_buf_details(FILE * fp, void *x)
{
    /* Write details to be called via a function pointer in atomic_write */
    struct buf *b = x;

    if (fwrite(b->a, 1, b->i, fp) != b->i)
        return 1;

    return 0;
}

int write_buf(struct buf *b, char *fn)
{
    return atomic_write(fn, b, write_buf_details);
}

int esyscmd(struct buf *input, struct buf *tmp_buf, char *cmd)
{
    FILE *fp;
    int x, status;
    delete_buf(tmp_buf);

#ifdef _WIN32
#define R_STR "rb"
#else
#define R_STR "r"
#endif
    if ((fp = popen(cmd, R_STR)) == NULL)
        return 1;

    errno = 0;
    while ((x = getc(fp)) != EOF)
        if (x != '\0' && unget_ch(tmp_buf, x)) {
            pclose(fp);
            return 1;
        }
    if (errno) {
        pclose(fp);
        return 1;
    }
    if ((status = pclose(fp)) == -1)
        return 1;
#ifdef _WIN32
    if (status)
        return 1;
#else
    if (!(WIFEXITED(status) && !WEXITSTATUS(status)))
        return 1;
#endif
    if (unget_ch(tmp_buf, '\0'))
        return 1;
    if (unget_str(input, tmp_buf->a))
        return 1;
    return 0;
}


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

int write_gapbuf_details(FILE * fp, void *x)
{
    /* Write details to be called via a function pointer in atomic_write */
    struct gapbuf *b = x;
    size_t n;

    /* Before gap */
    n = b->g - b->a;
    if (fwrite(b->a, 1, n, fp) != n)
        return 1;

    /* After gap, excluding the last character */
    n = b->e - b->c;
    if (fwrite(b->c, 1, n, fp) != n)
        return 1;

    return 0;
}

int write_file(struct gapbuf *b)
{
    if (atomic_write(b->fn, b, write_gapbuf_details))
        return 1;

    /* Success */
    b->mod = 0;
    return 0;
}

struct hashtable *init_hashtable(size_t num_buckets)
{
    struct hashtable *ht;
    if ((ht = calloc(1, sizeof(struct hashtable))) == NULL)
        return NULL;
    if ((ht->b = calloc(num_buckets, sizeof(struct entry *))) == NULL) {
        free(ht);
        return NULL;
    }
    ht->n = num_buckets;
    return ht;
}

void free_hashtable(struct hashtable *ht)
{
    struct entry *e, *ne;
    size_t j;
    if (ht != NULL) {
        if (ht->b != NULL) {
            for (j = 0; j < ht->n; ++j) {
                e = *(ht->b + j);
                while (e != NULL) {
                    ne = e->next;
                    free(e->name);
                    free(e->def);
                    free(e);
                    e = ne;
                }
            }
            free(ht->b);
        }
        free(ht);
    }
}

static struct entry *init_entry(void)
{
    struct entry *e;
    if ((e = malloc(sizeof(struct entry))) == NULL)
        return NULL;
    e->next = NULL;
    e->name = NULL;
    e->def = NULL;
    return e;
}

static size_t hash_str(struct hashtable *ht, char *s)
{
    /* djb2 */
    unsigned char c;
    size_t h = 5381;
    while ((c = *s++))
        h = h * 33 ^ c;
    return h % ht->n;
}

void htdist(struct hashtable *ht)
{
    struct entry *e;
    size_t freq[101] = { 0 }, count, k;
    for (k = 0; k < ht->n; ++k) {
        e = *(ht->b + k);
        count = 0;
        while (e != NULL) {
            e = e->next;
            ++count;
        }
        count < 100 ? ++*(freq + count) : ++*(freq + 100);
    }
    fprintf(stderr, "entries_per_bucket number_of_buckets\n");
    for (k = 0; k < 100; k++)
        if (*(freq + k))
            fprintf(stderr, "%lu %lu\n", (unsigned long) k,
                    (unsigned long) *(freq + k));
    if (*(freq + 100))
        fprintf(stderr, ">=100 %lu\n", (unsigned long) *(freq + 100));
}

struct entry *lookup_entry(struct hashtable *ht, char *name)
{
    size_t h = hash_str(ht, name);
    struct entry *e = *(ht->b + h);
    while (e != NULL) {
        /* Found */
        if (!strcmp(name, e->name))
            return e;
        e = e->next;
    }
    /* Not found */
    return NULL;
}

char *get_def(struct hashtable *ht, char *name)
{
    struct entry *e;
    if ((e = lookup_entry(ht, name)) == NULL)
        return NULL;
    return e->def;
}

int upsert_entry(struct hashtable *ht, char *name, char *def)
{
    struct entry *e;
    size_t h;
    char *t = NULL;
    if ((e = lookup_entry(ht, name)) == NULL) {
        /* Insert entry: */
        if ((e = init_entry()) == NULL)
            return 1;
        /* Store data */
        if ((e->name = strdup(name)) == NULL) {
            free(e);
            return 1;
        }
        if (def != NULL && (e->def = strdup(def)) == NULL) {
            free(e->name);
            free(e);
            return 1;
        }
        h = hash_str(ht, name);
        /* Link new entry in at head of list (existing head could be NULL) */
        e->next = *(ht->b + h);
        *(ht->b + h) = e;
    } else {
        /* Update entry: */
        if (def != NULL && (t = strdup(def)) == NULL)
            return 1;
        free(e->def);
        e->def = t;             /* Could be NULL */
    }
    return 0;
}

int delete_entry(struct hashtable *ht, char *name)
{
    size_t h = hash_str(ht, name);
    struct entry *e = *(ht->b + h), *prev = NULL;
    while (e != NULL) {
        /* Found */
        if (!strcmp(name, e->name))
            break;
        prev = e;
        e = e->next;
    }
    if (e != NULL) {
        /* Found */
        /* Link around the entry */
        if (prev != NULL)
            prev->next = e->next;
        free(e->name);
        free(e->def);
        free(e);
        /* At head of list */
        if (prev == NULL)
            *(ht->b + h) = NULL;
        return 0;
    }

    /* Not found */
    return 1;
}

int write_hashtable_details(FILE * fp, void *x)
{
    struct hashtable *ht = x;
    struct entry *e, *ne;
    size_t j;
    if (ht != NULL) {
        if (ht->b != NULL) {
            for (j = 0; j < ht->n; ++j) {
                e = *(ht->b + j);
                while (e != NULL) {
                    ne = e->next;
                    if (e->name != NULL && fprintf(fp, "%s", e->name) < 0)
                        return 1;
                    if (putc('\0', fp) == EOF)
                        return 1;
                    if (e->def != NULL && fprintf(fp, "%s", e->def) < 0)
                        return 1;
                    if (putc('\0', fp) == EOF)
                        return 1;
                    e = ne;
                }
            }
        }
    }
    return 0;
}

int write_hashtable(struct hashtable *ht, char *fn)
{
    /* Writes all the hash table entries to a file with a '\0' delimiter */
    if (atomic_write(fn, ht, write_hashtable_details))
        return 1;

    return 0;
}

int process_pair_ht(char *name, char *def, void *info)
{
    struct hashtable *ht = info;
    if (upsert_entry(ht, name, def))
        return 1;
    return 0;
}

int load_file_into_ht(struct hashtable *ht, char *fn)
{
    /* Load file with '\0' delimiter into hash table */
    return read_pair_file(fn, ht, process_pair_ht);
}


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

static int get_screen_size(size_t * height, size_t * width)
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

int endwin(void)
{
    int ret = 0;
    /* Screen is not initialised */
    if (stdscr == NULL)
        return 1;
    phy_attr_off();
    phy_clear_screen();
#ifndef _WIN32
    if (tcsetattr(STDIN_FILENO, TCSANOW, &stdscr->t_orig))
        ret = 1;
#endif
    free(stdscr->ns);
    free(stdscr->cs);
    free_buf(stdscr->input);
    free(stdscr);
    return ret;
}

WINDOW *initscr(void)
{
#ifdef _WIN32
    HANDLE out;
    DWORD mode;
#else
    struct termios term_orig, term_new;
#endif

    /* Error, screen is already initialised */
    if (stdscr != NULL)
        return NULL;

#ifdef _WIN32
    /* Check input is from a terminal */
    if (!_isatty(_fileno(stdin)))
        return NULL;
    /* Turn on interpretation of VT100-like escape sequences */
    if ((out = GetStdHandle(STD_OUTPUT_HANDLE)) == INVALID_HANDLE_VALUE)
        return NULL;
    if (!GetConsoleMode(out, &mode))
        return NULL;
    if (!SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        return NULL;
#else
    if (!isatty(STDIN_FILENO))
        return NULL;
    /* Change terminal input to raw and no echo */
    if (tcgetattr(STDIN_FILENO, &term_orig))
        return NULL;
    term_new = term_orig;
    cfmakeraw(&term_new);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &term_new))
        return NULL;
#endif

    if ((stdscr = calloc(1, sizeof(WINDOW))) == NULL) {
#ifndef _WIN32
        tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
#endif
        return NULL;
    }

    if ((stdscr->input = init_buf(INIT_BUF_SIZE)) == NULL) {
#ifndef _WIN32
        tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
#endif
        free(stdscr);
        stdscr = NULL;
        return NULL;
    }

    if (clear()) {
#ifndef _WIN32
        tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
#endif
        free_buf(stdscr->input);
        free(stdscr);
        stdscr = NULL;
        return NULL;
    }
#ifndef _WIN32
    stdscr->t_orig = term_orig;
#endif

    return stdscr;
}

static void draw_diff(void)
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

/* Raw getch */
#ifdef _WIN32
#define getch_raw() _getch()
#else
#define getch_raw() getchar()
#endif

/* Buffered getch with no key interpretation */
#define getch_nk() (stdscr->input->i ? *(stdscr->input->a + --stdscr->input->i) \
    : getch_raw())

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


static struct atom *compile_regex(char *find, struct cap_grp *cg,
                                  struct hook *hk)
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

static char *match_regex(struct atom *find, struct hook *hk, char *str,
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

static char *match_regex_here(struct atom *find, struct hook *hk,
                              char *str)
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

static char *match_regex_mult(struct atom *find, struct hook *hk,
                              char *str)
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

static void fill_in_capture_groups(struct cap_grp *cg, char *match_p,
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
        if ((t = strdup(str)) == NULL)
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
        if ((t = strdup(str)) == NULL) {
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


char *sha256(char *fn)
{
    /* Message block for SHA-256 is 512 bits (16 * 32-bit words) or 64 bytes */

    char *p, *hash_str, *hs, ch;
    unsigned char *q;
    FILE *fp;
    size_t fs, fs_left, mem_size, read_size, bytes_to_process;
    int le = 0;
    uint32_t test;
    unsigned char md[32], uch;
    uint32_t *z;
    size_t j;

    /* Variables from the standard */
    uint64_t l;
    uint32_t *M;
    uint32_t W[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t T1, T2;
    uint32_t H0, H1, H2, H3, H4, H5, H6, H7;
    size_t i, t;
    /* Number of 64 blocks in bytes_to_process, not the whole message */
    size_t N;

    uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b,
        0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74,
        0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3,
        0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354,
        0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
        0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3,
        0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa,
        0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    if (filesize(fn, &fs))
        return NULL;

    if (mof(fs, 8))
        return NULL;

    /* Message length in bits */
    l = fs * 8;

    /* Initial hash value */
    H0 = 0x6a09e667;
    H1 = 0xbb67ae85;
    H2 = 0x3c6ef372;
    H3 = 0xa54ff53a;
    H4 = 0x510e527f;
    H5 = 0x9b05688c;
    H6 = 0x1f83d9ab;
    H7 = 0x5be0cd19;

    /* Set the memory size */
    mem_size = min(BUFSIZ, fs);
    /* Check for possible overflow in the next two steps */
    if (aof(mem_size, 64 * 2))
        return NULL;
    /* Make a multiple of 64 */
    if (mem_size % 64)
        mem_size = (mem_size / 64 + 1) * 64;
    /* Add 64 bytes for padding */
    mem_size += 64;

    /* Open file for reading in binary mode */
    if ((fp = fopen(fn, "rb")) == NULL)
        return NULL;

    /* Allocate memory */
    if ((p = malloc(mem_size)) == NULL) {
        fclose(fp);
        return NULL;
    }

    M = (uint32_t *) p;

    /* Test if little endian */
    test = 0x01020304;
    q = (unsigned char *) &test;
    if (*q == 0x04)
        le = 1;

    fs_left = fs;

    /* Always run at least once (for empty files) */
    do {
        read_size = min(mem_size - 64, fs_left);

        if (fread(p, 1, read_size, fp) != read_size) {
            fclose(fp);
            free(p);
            return NULL;
        }

        fs_left -= read_size;

        if (!fs_left) {
            /* Padding the message */

            /*
             * Make the padded message a multiple of 64 bytes after adding 1
             * byte for the 0x80 byte, and 8 bytes for the total message size
             * in bits (expressed as an 8 byte integer).
             */
            if ((read_size + 1 + 8) % 64)
                bytes_to_process = ((read_size + 1 + 8) / 64 + 1) * 64;
            else
                bytes_to_process = read_size + 1 + 8;   /* Already a multiple */

            /* Zero out end of memmory */
            memset(p + read_size, '\0', bytes_to_process - read_size);

            /* Add the 1 to the end of the message */
            *(p + read_size) = (unsigned char) 0x80;

            /* Write the original message size in bits as an 8 byte integer */
            *((uint64_t *) (p + bytes_to_process - 8)) = l;

            /* Convert size to big endian */
            if (le) {
                j = bytes_to_process - 8;
                ch = p[j];
                p[j] = p[j + 7];
                p[j + 7] = ch;
                ch = p[j + 1];
                p[j + 1] = p[j + 6];
                p[j + 6] = ch;
                ch = p[j + 2];
                p[j + 2] = p[j + 5];
                p[j + 5] = ch;
                ch = p[j + 3];
                p[j + 3] = p[j + 4];
                p[j + 4] = ch;
            }
        } else {
            bytes_to_process = read_size;
        }

        /* Convert message to little endian */
        if (le)
            for (j = 0; j < bytes_to_process; j += 4) {
                ch = p[j];
                p[j] = p[j + 3];
                p[j + 3] = ch;
                ch = p[j + 1];
                p[j + 1] = p[j + 2];
                p[j + 2] = ch;
            }

        N = bytes_to_process / 64;

        /* For each 64 byte block in this read chunk */
        for (i = 0; i < N; ++i) {
            /* Prepare message schedule */
            for (t = 0; t <= 15; ++t)
                W[t] = *(M + i * 16 + t);
            for (t = 16; t <= 63; ++t)
                W[t] =
                    sigma1(W[t - 2]) + W[t - 7] + sigma0(W[t - 15]) + W[t -
                                                                        16];

            /* Initialise working variables */
            a = H0;
            b = H1;
            c = H2;
            d = H3;
            e = H4;
            f = H5;
            g = H6;
            h = H7;

            for (t = 0; t <= 63; ++t) {
                T1 = h + SIGMA1(e) + Ch(e, f, g) + K[t] + W[t];
                T2 = SIGMA0(a) + Maj(a, b, c);
                h = g;
                g = f;
                f = e;
                e = d + T1;
                d = c;
                c = b;
                b = a;
                a = T1 + T2;
            }

            /* Compute intermediate hash value */
            H0 += a;
            H1 += b;
            H2 += c;
            H3 += d;
            H4 += e;
            H5 += f;
            H6 += g;
            H7 += h;

        }
    } while (fs_left);

    free(p);

    if (fclose(fp))
        return NULL;

    /* Message digest */
    z = (uint32_t *) md;

    *z++ = H0;
    *z++ = H1;
    *z++ = H2;
    *z++ = H3;
    *z++ = H4;
    *z++ = H5;
    *z++ = H6;
    *z++ = H7;

    /* Convert message digest to big endian */
    if (le)
        for (j = 0; j < 32; j += 4) {
            uch = md[j];
            md[j] = md[j + 3];
            md[j + 3] = uch;
            uch = md[j + 1];
            md[j + 1] = md[j + 2];
            md[j + 2] = uch;
        }

    /* Dynamically allocate hash string */
    if ((hash_str = malloc(68)) == NULL)
        return NULL;

    /* Create a copy so can increment */
    hs = hash_str;

    /* Convert to hex */
    for (j = 0; j < 32; ++j) {
        uch = md[j];
        *hs++ = uch / 16 >= 10 ? 'a' + uch / 16 - 10 : '0' + uch / 16;
        *hs++ = uch % 16 >= 10 ? 'a' + uch % 16 - 10 : '0' + uch % 16;
    }
    /* Terminate string */
    *hs = '\0';

    return hash_str;
}
