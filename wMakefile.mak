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




all:
	cl /Ot \
		apps\spot\spot.c \
		mods\gen\gen.c \
		mods\buf\buf.c \
		mods\minicurses\minicurses.c \
        mods\regex\regex.c \
        mods\random\random.c \
		mods\fs\fs.c \
		mods\gapbuf\gapbuf.c
	cl /Ot \
		apps\m4\m4.c \
		mods\gen\gen.c \
		mods\buf\buf.c \
        mods\random\random.c \
        mods\fs\fs.c \
		mods\hashtable\hashtable.c \
        mods\regex\regex.c
	cl /Ot \
		apps\regr\regr.c \
		mods\gen\gen.c \
		mods\buf\buf.c \
        mods\random\random.c \
		mods\fs\fs.c \
		mods\regex\regex.c
	cl /Ot \
		apps\lsha256\lsha256.c \
		mods\gen\gen.c \
        mods\random\random.c \
		mods\fs\fs.c \
		mods\sha256\sha256.c
	cl /Ot \
        apps\capybara\capybara.c \
		mods\gen\gen.c \
        mods\random\random.c \
        mods\fs\fs.c \
		mods\sha256\sha256.c \
		mods\hashtable\hashtable.c \
		mods\buf\buf.c

clean:
	del *.exe *.obj
