#
# Copyright (c) 2021 Logan Ryan McLintock
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

F = -ansi -g -O3 -Wall -Wextra -pedantic

# Build fresh everytime
.PHONY: all
all:
	cc ${F} -o spot \
		mods/gen/gen.c \
		mods/buf/buf.c \
		mods/minicurses/minicurses.c \
        mods/regex/regex.c \
		mods/gapbuf/gapbuf.c \
		apps/spot/spot.c
	cc ${F} -o m4 \
		mods/gen/gen.c \
		mods/buf/buf.c \
		mods/hashtable/hashtable.c \
		apps/m4/m4.c
	cc ${F} -o regr \
		mods/gen/gen.c \
		mods/buf/buf.c \
		mods/regex/regex.c \
		apps/regr/regr.c

.PHONY: clean
clean:
	rm -f spot m4 regr
