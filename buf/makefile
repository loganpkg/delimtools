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
CFLAGS = -ansi -g -Wall -Wextra -pedantic
RM = rm -f

include_path = -I${PREFIX}/include
link_path = -L${PREFIX}/lib
libs = -lbuf -lncurses
cp = cp
mkdir = mkdir -p

spot: spot.c
	${CC} ${CFLAGS} ${include_path} ${link_path} -o spot spot.c ${libs}

.PHONY: clean
clean:
	${RM} spot

.PHONY: install
install:
	${mkdir} ${PREFIX}/bin
	${mkdir} ${PREFIX}/man/man1
	${cp} spot ${PREFIX}/bin/
	${cp} spot.1 ${PREFIX}/man/man1/
