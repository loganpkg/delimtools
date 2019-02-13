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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define LOGERR(m) (void) fprintf(stderr, "%s:%d: error: " m "\n",    \
					__FILE__, __LINE__)

#define BUF_SIZE 1024
#define NUM_CH UCHAR_MAX + 1

int main(int argc, char **argv)
{
	int ret = 0;
	FILE *fp = NULL;
	struct stat st;
	size_t filesize = 0;
	char *buf = NULL;
	ssize_t len;
	size_t read;
	size_t freq[NUM_CH] = { 0 };
	size_t i;

	if (argc != 1 && argc != 2) {
		fprintf(stderr, "Usage: %s: [file]\n", argv[0]);
		return 1;
	}

	if (argc == 1) {
		fp = stdin;
	} else {

		if (stat(argv[1], &st)) {
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
			return 1;
		}

		filesize = (size_t) st.st_size;

		if ((fp = fopen(argv[1], "r")) == NULL) {
			LOGERR("fopen failed");
			return 1;
		}
	}

	buf = malloc(BUF_SIZE);

	if (buf == NULL) {
		LOGERR("malloc failed");
		ret = 1;
		goto clean_up;
	}

	read = 0;
	while ((len = fread(buf, 1, BUF_SIZE, fp)) > 0) {
		read += len;
		for (i = 0; i < (size_t) len; ++i) {
			++freq[(int)buf[i]];
		}
	}

	if (read != filesize || !feof(fp) || ferror(fp)) {
		LOGERR("fread failed");
		ret = 1;
		goto clean_up;
	}

	for (i = 0; i < NUM_CH; ++i) {
		if (freq[i]) {
			if (isprint(i)) {
				printf("%d\t%c\t%lu\n", (int)i, (char)i,
				       freq[i]);
			} else {
				printf("%d\t%02x\t%lu\n", (int)i,
				       (unsigned char)i, freq[i]);
			}
		}
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
