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
