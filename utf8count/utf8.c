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
 * utf8lib.c
 */

#include <stdint.h>
#include <stdio.h>

#include "utf8.h"

int ufreq(char *line, size_t len, size_t * cp_freq)
{
	unsigned char uc;
	uint32_t cp = 0;
	int contin;
	size_t i;

	contin = 0;
	for (i = 0; i < len; ++i) {
		uc = (unsigned char)line[i];

		if (ISASCII(uc)) {
			if (contin) {
				return 1;
			}
			cp = uc;
		} else if (ISLEAD2(uc)) {
			if (contin) {
				return 1;
			}
			cp = uc & 0x1F;
			contin = 1;
		} else if (ISLEAD3(uc)) {
			if (contin) {
				return 1;
			}
			cp = uc & 0x0F;
			contin = 2;
		} else if (ISLEAD4(uc)) {
			if (contin) {
				return 1;
			}
			cp = uc & 0x07;
			contin = 3;
		} else if (ISCONTIN(uc)) {
			switch (contin) {
			case 3:
			case 2:
			case 1:
				cp = cp << 6 | (uc & 0x3F);
				break;
			default:
				return 1;
			}
			--contin;

		} else {
			return 1;
		}

		if (cp > MAXCP) {
			return 1;
		}

		if (!contin) {
			++cp_freq[cp];
		}
	}

	if (contin) {
		return 1;
	}

	return 0;
}

int uprintcp(uint32_t cp)
{
	if (ISASCII(cp)) {
		putchar(cp);

	} else if (ISSIZE2(cp)) {
		putchar((cp >> 6 & 0x1F) | 0xC0);
		putchar((cp & 0x3F) | 0x80);

	} else if (ISSIZE3(cp)) {
		putchar((cp >> 12 & 0x0F) | 0xE0);
		putchar((cp >> 6 & 0x3F) | 0x80);
		putchar((cp & 0x3F) | 0x80);

	} else if (ISSIZE4(cp)) {
		putchar((cp >> 18 & 0x07) | 0xF0);
		putchar((cp >> 12 & 0x3F) | 0x80);
		putchar((cp >> 6 & 0x3F) | 0x80);
		putchar((cp & 0x3F) | 0x80);

	} else {
		return 1;
	}

	return 0;
}
