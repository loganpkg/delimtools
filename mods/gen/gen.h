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


#ifndef GEN_H
#define GEN_H

/* For SIZE_MAX */
#include <stdint.h>


#ifdef _WIN32
/* For SSIZE_T */
#include <BaseTsd.h>

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

#define strlcpy(dst, src, d_size) snprintf(dst, d_size, "%s", src)
#define reallocarray(p, num, size) (mof(num, size) ? NULL : realloc(p, (num) * (size)))

int str_to_num(char *s, size_t * num);
char *memmatch(char *big, size_t big_len, char *little, size_t little_len);
char *concat(char *str0, ...);

#endif
