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

libgen.so: gen.o
	${CC} ${linker_opts} -o libgen.so gen.o

gen.o: gen.h gen.c
	${CC} ${CFLAGS} gen.c

.PHONY: clean
clean:
	${RM} gen.o libgen.so

.PHONY: install
install:
	${mkdir} ${PREFIX}/include
	${mkdir} ${PREFIX}/lib
	$(cp) gen.h ${PREFIX}/include/
	$(cp) libgen.so ${PREFIX}/lib/
