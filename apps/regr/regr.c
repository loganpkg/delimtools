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

/* regr: Regular expression find and replace */

#include <stdio.h>
#include <stdlib.h>

#include "../../mods/gen/gen.h"
#include "../../mods/regex/regex.h"
#include "../../mods/fs/fs.h"

int main(int argc, char **argv)
{
    int i;
    char *find, *replace, *str, *res;

    if (argc < 4) {
       fprintf(stderr, "Usage: %s find replace file...\n", *argv);
       return 1;
    }

    find = *(argv + 1);
    replace = *(argv + 2);

    for (i = 3; i < argc; ++i) {
        if ((str = file_to_str(*(argv + i))) == NULL) return 1;
        if ((res = regex_replace(str, find, replace, 1)) == NULL) {
            free(str);
            return 1;
        }
        free(str);
        printf("%s", res);
        free(res);
    }
    
return 0;
}
