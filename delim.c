/*
 * Copyright (c) 2021, 2022 Logan Ryan McLintock
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

/* delim: Checks a delimiter */

#ifdef _WIN32
/* For _O_BINARY */
#include <fcntl.h>
/* For _setmode */
#include <io.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int sane_standard_streams(void)
{
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1)
        return 1;
    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        return 1;
    if (_setmode(_fileno(stderr), _O_BINARY) == -1)
        return 1;
#endif
    return 0;
}

int main(int argc, char **argv)
{
    FILE *fp = NULL;
    int stdin_read;
    unsigned long int row, first_count, count;
    unsigned char *chunk;
    int last;
    int i;
    size_t r, j;
    char delim;
    int first_line;             /* Gets reset each new file */

    if (sane_standard_streams())
        return 1;

    if ((chunk = malloc(BUFSIZ)) == NULL)
        return 1;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s delimiter [files]\n", *argv);
        return 1;
    }

    if (strlen(*(argv + 1)) != 1) {
        fprintf(stderr,
                "%s: Delimiter must be a single character\n",
                *argv);
        return 1;
    }

    if (**(argv + 1) == '\n') {
        fprintf(stderr,
                "%s: Delimiter cannot be the newline character (Line Feed)\n",
                *argv);
        return 1;
    }

    delim = **(argv + 1);

    if (argc == 2) {
        /* No files given, so read from stdin */
        stdin_read = 1;
        fp = stdin;
    } else {
        stdin_read = 0;
    }

    i = 2;
    do {
        /* Set this for each new file */
        first_line = 1;
        row = 1;
        first_count = 0;
        count = 0;

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

            /* Process char by char */
            for (j = 0; j < r; ++j) {
                if (chunk[j] == delim)
                    ++count;

                if (chunk[j] == '\n' || (last && j == r - 1)) {
                    /* End of line or end of file, as not all files end in \n */
                    if (first_line) {
                        /* Record the delimiter count for the first line */
                        first_count = count;
                        /* Turn off indicator */
                        first_line = 0;
                    } else if (count != first_count) {
                        fprintf(stderr,
                                "%s: Delimiter mismatch."
                                " First line of %s has %lu,"
                                " line %lu has %lu.\n",
                                *argv, stdin_read ? "stdin" : *(argv + i),
                                first_count, row, count);
                        return 1;
                    }
                    ++row;
                    /* Reset count for next line */
                    count = 0;
                }
            }

        } while (!last);

        /* Close the file */
        if (!stdin_read && fclose(fp)) {
            free(chunk);
            return 1;
        }

    } while (!stdin_read && ++i < argc);

    /* Clean up */
    free(chunk);

    return 0;
}
