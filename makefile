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

# Change this to install somewhere else
PREFIX = ${HOME}

.PHONY: all
all: spot m4 regr lsha256 capybara freq delim

spot: gen.o buf.o minicurses.o regex.o random.o fs.o gapbuf.o apps/spot/spot.c
	cc ${F} -o spot gen.o buf.o minicurses.o regex.o random.o fs.o gapbuf.o apps/spot/spot.c

m4: gen.o buf.o random.o fs.o hashtable.o regex.o apps/m4/m4.c
	cc ${F} -o m4 gen.o buf.o random.o fs.o hashtable.o regex.o apps/m4/m4.c

regr: gen.o buf.o random.o fs.o regex.o apps/regr/regr.c
	cc ${F} -o regr gen.o buf.o random.o fs.o regex.o apps/regr/regr.c

lsha256: gen.o random.o fs.o sha256.o apps/lsha256/lsha256.c
	cc ${F} -o lsha256 gen.o random.o fs.o sha256.o apps/lsha256/lsha256.c

freq: mods/gen/gen.h apps/freq/freq.c
	cc ${F} -o freq apps/freq/freq.c

delim: mods/gen/gen.h apps/delim/delim.c
	cc ${F} -o delim apps/delim/delim.c

gen.o: mods/gen/gen.h mods/gen/gen.c
	cc -c ${F} mods/gen/gen.c

buf.o: mods/gen/gen.h mods/fs/fs.h \
	mods/buf/buf.h mods/buf/buf.c
	cc -c ${F} mods/buf/buf.c

gapbuf.o: mods/gen/gen.h mods/buf/buf.h mods/regex/regex.h mods/fs/fs.h \
	mods/gapbuf/gapbuf.h mods/gapbuf/gapbuf.c
	cc -c ${F} mods/gapbuf/gapbuf.c

hashtable.o: mods/gen/gen.h mods/fs/fs.h \
	mods/hashtable/hashtable.h mods/hashtable/hashtable.c
	cc -c ${F} mods/hashtable/hashtable.c

minicurses.o: mods/gen/gen.h mods/buf/buf.h \
	mods/minicurses/minicurses.h mods/minicurses/minicurses.c
	cc -c ${F} mods/minicurses/minicurses.c

random.o: mods/gen/gen.h \
	mods/random/random.h mods/random/random.c
	cc -c ${F} mods/random/random.c

fs.o: mods/gen/gen.h mods/random/random.h \
	mods/fs/fs.h mods/fs/fs.c
	cc -c ${F} mods/fs/fs.c

sha256.o: mods/gen/gen.h mods/fs/fs.h \
	mods/sha256/sha256.h mods/sha256/sha256.c
	cc -c ${F} mods/sha256/sha256.c

regex.o: mods/gen/gen.h mods/buf/buf.h \
	mods/regex/regex.h mods/regex/regex.c
	cc -c ${F} mods/regex/regex.c

.PHONY: install
install:
	mv spot m4 regr lsha256 capybara freq delim ${PREFIX}/bin/

.PHONY: clean
clean:
	rm -f gen.o buf.o gapbuf.o hashtable.o minicurses.o random.o fs.o sha256.o regex.o \
	spot m4 regr lsha256 capybara freq delim
