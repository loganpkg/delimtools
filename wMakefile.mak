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

PREFIX = %HOMEPATH%

all: spot.exe m4.exe regr.exe lsha256.exe freq.exe delim.exe

spot.exe: gen.obj buf.obj minicurses.obj regex.obj random.obj fs.obj gapbuf.obj apps\spot\spot.c
        cl /Ot apps\spot\spot.c gen.obj buf.obj minicurses.obj regex.obj random.obj fs.obj gapbuf.obj

m4.exe: gen.obj buf.obj random.obj fs.obj hashtable.obj regex.obj apps\m4\m4.c
        cl /Ot apps\m4\m4.c gen.obj buf.obj random.obj fs.obj hashtable.obj regex.obj

regr.exe: gen.obj buf.obj random.obj fs.obj regex.obj apps\regr\regr.c
        cl /Ot apps\regr\regr.c gen.obj buf.obj random.obj fs.obj regex.obj

lsha256.exe: gen.obj random.obj fs.obj sha256.obj apps\lsha256\lsha256.c
        cl /Ot apps\lsha256\lsha256.c gen.obj random.obj fs.obj sha256.obj

freq.exe: mods\gen\gen.h apps\freq\freq.c
        cl /Ot apps\freq\freq.c

delim.exe: mods\gen\gen.h apps\delim\delim.c
        cl /Ot apps\delim\delim.c

gen.obj: mods\gen\gen.h mods\gen\gen.c
        cl /c /Ot mods\gen\gen.c

buf.obj: mods\gen\gen.h mods\fs\fs.h \
        mods\buf\buf.h mods\buf\buf.c
        cl /c /Ot mods\buf\buf.c

gapbuf.obj: mods\gen\gen.h mods\buf\buf.h mods\regex\regex.h mods\fs\fs.h \
        mods\gapbuf\gapbuf.h mods\gapbuf\gapbuf.c
        cl /c /Ot mods\gapbuf\gapbuf.c

hashtable.obj: mods\gen\gen.h mods\fs\fs.h \
        mods\hashtable\hashtable.h mods\hashtable\hashtable.c
        cl /c /Ot mods\hashtable\hashtable.c

minicurses.obj: mods\gen\gen.h mods\buf\buf.h \
        mods\minicurses\minicurses.h mods\minicurses\minicurses.c
        cl /c /Ot mods\minicurses\minicurses.c

random.obj: mods\gen\gen.h \
        mods\random\random.h mods\random\random.c
        cl /c /Ot mods\random\random.c

fs.obj: mods\gen\gen.h mods\random\random.h \
        mods\fs\fs.h mods\fs\fs.c
        cl /c /Ot mods\fs\fs.c

sha256.obj: mods\gen\gen.h mods\fs\fs.h \
        mods\sha256\sha256.h mods\sha256\sha256.c
        cl /c /Ot mods\sha256\sha256.c

regex.obj: mods\gen\gen.h mods\buf\buf.h \
        mods\regex\regex.h mods\regex\regex.c
        cl /c /Ot mods\regex\regex.c

install:
        move *.exe $(PREFIX)\bin\

clean:
        del *.exe *.obj
