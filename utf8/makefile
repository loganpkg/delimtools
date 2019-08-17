#
# Copyright (c) 2019 Logan Ryan McLintock
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


CC = cc
CFLAGS = -ansi -O2 -g -Wall -Wextra -pedantic
RM = rm -f

cp = cp
mkdir = mkdir -p

utf8count: utf8count.c utf8.c utf8.h
	${CC} ${CFLAGS} -o utf8count utf8count.c utf8.c

.PHONY: clean
clean:
	${RM} utf8count

.PHONY: install
install:
	${mkdir} ${PREFIX}/bin
	${cp} utf8count ${PREFIX}/bin/
