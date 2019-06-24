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
 * uniqrand - unique uniformly distributed random numbers in a random order
 * $ cc -ansi -O2 -g -Wall -Wextra -pedantic -o uniqrand uniqrand.c -lbsd
 */

#include <bsd/stdlib.h>
#include <stdlib.h>

#define LOG(m) fprintf(stderr, "%s:%d: error: " m "\n", __FILE__, __LINE__)

int main(int argc, char **argv)
{
	u_int32_t lower_inc, upper_exc, num, needed, window_s, i, j, temp;
	const char *error_str = NULL;
	u_int32_t *a = NULL;

	if (argc != 4) {
		fprintf(stderr, "Usage: %s lower_inc upper_exc num\n", argv[0]);
		return 1;
	}

	lower_inc = strtonum(argv[1], 0, UINT32_MAX, &error_str);
	if (error_str != NULL) {
		LOG("strtonum failed");
		return 1;
	}

	upper_exc = strtonum(argv[2], 0, UINT32_MAX, &error_str);
	if (error_str != NULL) {
		LOG("strtonum failed");
		return 1;
	}

	num =
	    strtonum(argv[3], 0,
		     UINT32_MAX >
		     SIZE_MAX / sizeof(u_int32_t) ? SIZE_MAX /
		     sizeof(u_int32_t) : UINT32_MAX, &error_str);
	if (error_str != NULL) {
		LOG("strtonum failed");
		return 1;
	}

	if (!num || upper_exc <= lower_inc || num > upper_exc - lower_inc) {
		LOG("invalid input");
		return 1;
	}

	if ((a = malloc(num * sizeof(u_int32_t))) == NULL) {
		LOG("malloc failed");
		return 1;
	}

	/* unique uniformly distributed random numbers */
	needed = num;
	window_s = upper_exc - lower_inc;
	while (window_s) {
		if (arc4random_uniform(window_s) < needed) {
			a[needed - 1] = lower_inc + window_s - 1;
			--needed;
		}
		--window_s;
	}

	/* Random shuffle */
	i = num - 1;
	while (i) {
		j = arc4random_uniform(i + 1);
		/* Swap */
		temp = a[i];
		a[i] = a[j];
		a[j] = temp;
		--i;
	}

	/* Print results */
	for (i = 0; i < num; i++)
		printf("%u,%u\n", i + 1, a[i]);

	free(a);
	a = NULL;

	return 0;
}
