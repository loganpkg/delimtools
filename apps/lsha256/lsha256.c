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

/* SHA-256 utility */

#include <stdio.h>
#include <stdlib.h>

#include "../../mods/sha256/sha256.h"

int main(int argc, char **argv)
{
    char *hash;
    int i;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s file...\n", *argv);
        return 1;
    }

    for (i = 1; i < argc; ++i) {
        if ((hash = sha256(*(argv + i))) == NULL)
            return 1;
        printf("%s %s\n", hash, *(argv + i));
        free(hash);
    }

    return 0;
}
