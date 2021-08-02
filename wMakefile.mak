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
	cl /Ot /Wall \
		/wd4820 /wd4464 /wd4244 /wd4310 /wd4267 /wd4996 /wd4242 /wd4710 /wd4255 \
		/wd5045 /wd4090 /wd4706 \
		/DNDEBUG \
		apps\spot\spot.c \
		mods\gen\gen.c \
		mods\buf\buf.c \
		mods\minicurses\minicurses.c \
        mods\regex\regcomp.c \
		mods\regex\regerror.c \
		mods\regex\regexec.c \
		mods\regex\regfree.c \
		mods\rx\rx.c \
		mods\gapbuf\gapbuf.c
	cl /Ot /Wall \
		/wd4820 /wd4464 /wd4996 /wd4710 /wd5045 /wd4242 /wd4244 /wd4267 /wd4706 \
		apps\m4\m4.c \
		mods\gen\gen.c \
		mods\buf\buf.c \
		mods\hashtable\hashtable.c

clean:
	del *.exe *.obj
