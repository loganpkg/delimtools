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


#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include "gen.h"


int safeadd(size_t * res, int num_args, ...)
{
	va_list ap;
	int i;
	size_t total = 0;
	size_t arg_val = 0;

	va_start(ap, num_args);
	for (i = 0; i < num_args; ++i) {
		arg_val = va_arg(ap, size_t);
		if (ADDOF(total, arg_val)) {
			LOG("integer overflow");
			va_end(ap);
			return -1;
		}
		total += arg_val;
	}
	va_end(ap);
	*res = total;
	return 0;
}

int filesize(size_t * fs, char *fn)
{
	struct stat st;

	if (fn == NULL || !strlen(fn)) {
		LOG("empty filename");
		return -1;
	}

	if (stat(fn, &st)) {
		LOG("stat failed");
		return -1;
	}

	if (!S_ISREG(st.st_mode)) {
		LOG("not regular file");
		return -1;
	}

	if (st.st_size < 0) {
		LOG("negative file size");
		return -1;
	}

	*fs = (size_t) st.st_size;
	return 0;
}

int ucount(char *line, size_t len, size_t * cp_count)
{
	unsigned char uc;
	uint32_t cp = 0;
	int contin;
	size_t i;

	contin = 0;
	for (i = 0; i < len; ++i) {
		uc = (unsigned char)line[i];

		if (ISASCII(uc)) {
			if (contin)
				return 1;

			cp = uc;

		} else if (ISLEAD2(uc)) {
			if (contin)
				return 1;

			cp = uc & 0x1F;
			contin = 1;

		} else if (ISLEAD3(uc)) {
			if (contin)
				return 1;

			cp = uc & 0x0F;
			contin = 2;

		} else if (ISLEAD4(uc)) {
			if (contin)
				return 1;

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

		if (cp > MAXCP)
			return 1;

		if (!contin)
			++cp_count[cp];

	}

	if (contin)
		return 1;

	return 0;
}

int ucptostr(uint32_t cp, char *utf8chstr)
{
	if (ISASCII(cp)) {
		utf8chstr[0] = cp;
		utf8chstr[1] = '\0';

	} else if (ISSIZE2(cp)) {
		utf8chstr[0] = (cp >>  6 & 0x1F) | 0xC0;
		utf8chstr[1] = (cp       & 0x3F) | 0x80;
		utf8chstr[2] = '\0';

	} else if (ISSIZE3(cp)) {
		utf8chstr[0] = (cp >> 12 & 0x0F) | 0xE0;
		utf8chstr[1] = (cp >>  6 & 0x3F) | 0x80;
		utf8chstr[2] = (cp       & 0x3F) | 0x80;
		utf8chstr[3] = '\0';

	} else if (ISSIZE4(cp)) {
		utf8chstr[0] = (cp >> 18 & 0x07) | 0xF0;
		utf8chstr[1] = (cp >> 12 & 0x3F) | 0x80;
		utf8chstr[2] = (cp >>  6 & 0x3F) | 0x80;
		utf8chstr[3] = (cp       & 0x3F) | 0x80;
		utf8chstr[4] = '\0';

	} else {
		return 1;
	}

	return 0;
}

int hextonum(int h, int *num)
{
	if (!isxdigit(h))
		return 1;

	if (isdigit(h))
		*num = h - '0';
	else if (islower(h))
		*num = h - 'a' + 10;
	else
		*num = h - 'A' + 10;

	return 0;
}

int strtochar(char *str, char *ch)
{
	size_t len;
	int h0, h1;

	if (str == NULL)
		return 1;

	len = strlen(str);

	/* C hexadecimal escape sequence */
	if (len == 4 && str[0] == '\\' && str[1] == 'x') {
		if (hextonum(str[2], &h0) || hextonum(str[3], &h1)) {
			LOG("invalid hexadecimal digit");
			return 1;
		}

		*ch = 16 * h0 + h1;
		return 0;
	}

	/* C single character escape sequence */
	if (len == 2 && str[0] == '\\') {
		switch (str[1]) {
		case '0':
			*ch = '\0';
			break;
		case 'a':
			*ch = '\a';
			break;
		case 'b':
			*ch = '\b';
			break;
		case 't':
			*ch = '\t';
			break;
		case 'n':
			*ch = '\n';
			break;
		case 'v':
			*ch = '\v';
			break;
		case 'f':
			*ch = '\f';
			break;
		case 'r':
			*ch = '\r';
			break;
		case 'e':
			/* Not standard C */
			*ch = 27;
			break;
		case '\\':
			*ch = '\\';
			break;
		case '\'':
			*ch = '\'';
			break;
		case '"':
			*ch = '"';
			break;
		case '?':
			*ch = '?';
			break;
		default:
			LOG("unrecognised C single character escape sequence");
			return 1;
		}
		return 0;
	}

	/* Single char */
	if (len == 1) {
		*ch = str[0];
		return 0;
	}

	LOG("unrecognised string");
	return 1;
}
