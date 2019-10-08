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
CFLAGS = -c -ansi -g -O2 -Wall -Wextra -pedantic -fpic
RM = rm -f

linker_opts = -shared
mkdir = mkdir -p
cp = cp

libsutf8.so: sutf8.o
	${CC} ${linker_opts} -o libsutf8.so sutf8.o

sutf8.o: sutf8.h sutf8.c
	${CC} ${CFLAGS} sutf8.c

.PHONY: clean
clean:
	${RM} sutf8.o libsutf8.so

.PHONY: install
install:
	${mkdir} ${PREFIX}/include
	${mkdir} ${PREFIX}/lib
	$(cp) sutf8.h ${PREFIX}/include/
	$(cp) libsutf8.so ${PREFIX}/lib/
