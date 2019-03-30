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

#define _GNU_SOURCE
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <macros.h>

#define INIT_BUF_SIZE 1024

int main(int argc, char **argv)
{
	int ret = 0;
	FILE *fp = NULL;
	char *buf = NULL;
	size_t buf_size = 0;
	ssize_t line_len;
	size_t row_count;
	char *start;
	char *match;
	size_t delim_count;
	size_t first_delim_count;

	struct stat st;
	char *delim_str;
	size_t delim_len;

	if (argc != 2 && argc != 3) {
		fprintf(stderr, "Usage: %s: delimiter [file]\n", argv[0]);
		return 1;
	}

	delim_str = argv[1];

	if (!strlen(delim_str)) {
		LOGERR("empty delimiter");
		return 1;
	} else if (!strcmp(delim_str, "\\t")) {
		delim_str[0] = '\t';
		delim_str[1] = '\0';
	} else if (!strcmp(delim_str, "\\n") || !strcmp(delim_str, "\n")) {
		LOGERR("delimiter cannot be a line feed character");
		return 1;
	}

	delim_len = strlen(delim_str);

	if (argc == 2 || !strcmp(argv[2], "-")) {
		fp = stdin;
	} else {

		if (stat(argv[2], &st)) {
			LOGERR("stat failed");
			return 1;
		}

		if (!S_ISREG(st.st_mode)) {
			LOGERR("not a regular file");
			return 1;
		}

		if (st.st_size < 0) {
			LOGERR("negative file size");
			return 1;
		}

		if (st.st_size == 0) {
			return 0;
		}

		if ((fp = fopen(argv[2], "r")) == NULL) {
			LOGERR("fopen failed");
			return 1;
		}
	}

	buf = malloc(INIT_BUF_SIZE);

	if (buf == NULL) {
		LOGERR("malloc failed");
		ret = 1;
		goto clean_up;
	}

	buf_size = INIT_BUF_SIZE;

	row_count = 0;
	delim_count = 0;

	while ((line_len = getline(&buf, &buf_size, fp)) > 0) {
		++row_count;

		start = buf;

		while ((match =
			memmem(start, (size_t) line_len, delim_str,
			       delim_len)) != NULL) {
			++delim_count;
			if (match - buf != line_len) {
				start = match + 1;
			} else {
				break;
			}
		}

		if (row_count == 1) {
			first_delim_count = delim_count;
		} else if (delim_count != first_delim_count) {
			fprintf(stderr,
				"%s: Delimiter mismatch:\nLine 1: %lu\nLine %lu: %lu\n",
				argv[0], first_delim_count, row_count,
				delim_count);
			ret = 1;
			goto clean_up;
		}

		delim_count = 0;
	}

 clean_up:
	if (fp != NULL && fp != stdin) {
		if (fclose(fp)) {
			LOGERR("fclose failed");
			ret = 1;
		}
	}

	if (buf != NULL) {
		free(buf);
	}

	return ret;
}
