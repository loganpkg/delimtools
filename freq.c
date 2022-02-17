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

/* freq: Character frequency */

#include "use_toucan.h"

int main(int argc, char **argv)
{
    FILE *fp = NULL;
    int stdin_read;
    unsigned long int freq[UCHAR_NUM] = { 0 };
    unsigned char *chunk;
    int last;
    int i, k;
    size_t r, j;

    if (sane_standard_streams())
        return 1;

    if ((chunk = malloc(BUFSIZ)) == NULL)
        return 1;

    if (argc == 2 && !strcmp(*(argv + 1), "-h")) {
        fprintf(stderr, "Usage: %s [files]\n", *argv);
        return 0;
    }

    if (argc == 1) {
        /* No files given, so read from stdin */
        stdin_read = 1;
        fp = stdin;
    } else {
        stdin_read = 0;
    }

    i = 1;
    do {
        /* Open the file */
        if (!stdin_read && (fp = fopen(*(argv + i), "rb")) == NULL) {
            free(chunk);
            return 1;
        }

        last = 0;
        do {
            /* Read chunks until EOF */
            if ((r = fread(chunk, 1, BUFSIZ, fp)) != BUFSIZ) {
                if (!ferror(fp) && feof(fp)) {
                    last = 1;
                } else {
                    /* A read error occurred */
                    free(chunk);
                    if (!stdin_read)
                        fclose(fp);

                    return 1;
                }
            }

            /* Process character frequency of the chunk */
            for (j = 0; j < r; ++j)
                ++freq[chunk[j]];

        } while (!last);

        /* Close the file */
        if (!stdin_read && fclose(fp)) {
            free(chunk);
            return 1;
        }

    } while (!stdin_read && ++i < argc);

    /* Print the results */
    for (k = 0; k < UCHAR_NUM; ++k)
        if (freq[k])
            printf(isgraph(k) ? "%c\t%lu\n" : "%02X\t%lu\n", k, freq[k]);

    /* Clean up */
    free(chunk);

    return 0;
}
