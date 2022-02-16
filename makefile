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

# Change this to install somewhere else
INSTALL_DIR = ${HOME}/bin

F = -ansi -g -O3 -Wall -Wextra -pedantic

.PHONY: all
all: spot m4 regr lsha256 freq delim llama

spot: toucan.o spot.c
	cc ${F} -o spot toucan.o spot.c

m4: toucan.o m4.c
	cc ${F} -o m4 toucan.o m4.c

regr: toucan.o regr.c
	cc ${F} -o regr toucan.o regr.c

lsha256: toucan.o lsha256.c
	cc ${F} -o lsha256 toucan.o lsha256.c

freq: toucan.o freq.c
	cc ${F} -o freq toucan.o freq.c

delim: toucan.o delim.c
	cc ${F} -o delim toucan.o delim.c

llama: toucan.o llama.c
	cc ${F} -o llama toucan.o llama.c

toucan.o: toucan.h toucan.c
	cc -c ${F} toucan.c

.PHONY: install
install:
	mkdir -p ${INSTALL_DIR}
	cp -p spot m4 regr lsha256 freq delim ${INSTALL_DIR}

.PHONY: clean
clean:
	rm -f toucan.o spot m4 regr lsha256 freq delim llama
