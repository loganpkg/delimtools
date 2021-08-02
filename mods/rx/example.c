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

/*
   $ cc -ansi -g -O3 -Wall -Wextra -pedantic -DRR_DEBUG ../gen/gen.c \
     ../buf/buf.c ../regex/regcomp.c ../regex/regerror.c ../regex/regexec.c \
     ../regex/regfree.c rx.c example.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#include "rx.h"

int main(void)
{
    char *str = "cool\n\n\n\n\nzoo";
    struct buf *r;

    printf("========\n%s\n========\n", str);
    if ((r = regex_replace("$", "rabbit", str, 1)) == NULL)
        return 1;
    printf("========\n%s\n========\n", r->a);
    free_buf(r);

    return 0;
}
