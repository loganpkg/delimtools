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
 * Generic C library.
 */

#ifndef GEN_H_
#define GEN_H_

/* Error message */
#define LOG(m) fprintf(stderr, "%s:%d: error: " m "\n", __FILE__, __LINE__)
/* size_t addition overflow test */
#define ADDOF(a, b)  ((a) > SIZE_MAX - (b))
/* size_t multiplication overflow test */
#define MULTOF(a, b) ((a) && (b) > SIZE_MAX / (a))

/* Control characters */
#define Cspc 0
#define Ca 1
#define Cb 2
#define Cc 3
#define Cd 4
#define Ce 5
#define Cf 6
#define Cg 7
#define Ch 8
#define Ci 9
#define Cj 10
#define Ck 11
#define Cl 12
#define Cm 13
#define Cn 14
#define Co 15
#define Cp 16
#define Cq 17
#define Cr 18
#define Cs 19
#define Ct 20
#define Cu 21
#define Cv 22
#define Cw 23
#define Cx 24
#define Cy 25
#define Cz 26
#define ESC 27
#define Cqm 127

#define ISASCII(u) ((u) < 0x80)

#define ISCONTIN(uc) ((uc) >= 0x80 && (uc) < 0xC0)
#define ISLEAD2(uc)  ((uc) >= 0xC0 && (uc) < 0xE0)
#define ISLEAD3(uc)  ((uc) >= 0xE0 && (uc) < 0xF0)
#define ISLEAD4(uc)  ((uc) >= 0xF0 && (uc) < 0xF8)
#define ISINVAL(uc)  ((uc) >= 0xF8)

#define MAXCONTIN 3

#define NUMCP 0x110000
#define MAXCP (NUMCP - 1)

#define ISSIZE2(cp) ((cp) >= 0x80    && (cp) < 0x800)
#define ISSIZE3(cp) ((cp) >= 0x800   && (cp) < 0x10000)
#define ISSIZE4(cp) ((cp) >= 0x10000 && (cp) < NUMCP)

int safeadd(size_t * res, int num_args, ...);
int filesize(size_t * fs, char *fn);
int ucount(char *line, size_t len, size_t * cp_count);
int ucptostr(uint32_t cp, char *utf8chstr);
int hextonum(char h, int *num);
int strtochar(char *str, char *ch);

#endif
