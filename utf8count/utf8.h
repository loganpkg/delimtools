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

/* UTF8 header */

#ifndef UTF8_H_
#define UTF8_H_

#include <stdint.h>

#define ISASCII(u) ((u) < 0x80)

#define ISCONTIN(uc) ((uc) >= 0x80 && (uc) < 0xC0)
#define ISLEAD2(uc) ((uc) >= 0xC0 && (uc) < 0xE0)
#define ISLEAD3(uc) ((uc) >= 0xE0 && (uc) < 0xF0)
#define ISLEAD4(uc) ((uc) >= 0xF0 && (uc) < 0xF8)
#define ISINVAL(uc) ((uc) >= 0xF8)

#define MAXCONTIN 3

#define NUMCP 0x110000
#define MAXCP (NUMCP - 1)

#define ISSIZE2(cp) ((cp) >= 0x80 && (cp) < 0x800)
#define ISSIZE3(cp) ((cp) >= 0x800 && (cp) < 0x10000)
#define ISSIZE4(cp) ((cp) >= 0x10000 && (cp) < NUMCP)

int ucount(char *line, size_t len, size_t * cp_count);
int ucptostr(uint32_t cp, char *utf8chstr);

#endif
