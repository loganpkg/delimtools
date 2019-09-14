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
 * strtochar -- converts a string to a single char
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "strtochar.h"

int hextonum(char h, int *num)
{
	switch (h) {
	case '0':
		*num = 0;
		break;
	case '1':
		*num = 1;
		break;
	case '2':
		*num = 2;
		break;
	case '3':
		*num = 3;
		break;
	case '4':
		*num = 4;
		break;
	case '5':
		*num = 5;
		break;
	case '6':
		*num = 6;
		break;
	case '7':
		*num = 7;
		break;
	case '8':
		*num = 8;
		break;
	case '9':
		*num = 9;
		break;
	case 'a':
	case 'A':
		*num = 10;
		break;
	case 'b':
	case 'B':
		*num = 11;
		break;
	case 'c':
	case 'C':
		*num = 12;
		break;
	case 'd':
	case 'D':
		*num = 13;
		break;
	case 'e':
	case 'E':
		*num = 14;
		break;
	case 'f':
	case 'F':
		*num = 15;
		break;
	default:
		return 1;
	}

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
