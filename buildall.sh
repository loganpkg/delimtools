#!/bin/sh

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

set -x

changequote(`[', `]')


define([installdir], ["$HOME"])

define([os], esyscmd(uname -s | tr -d '\n'))

define([std_cc_flags], [-ansi -g -O2 -Wall -Wextra -pedantic])

define([bilib], [cd $1
cc -c std_cc_flags -fpic -I installdir/include -L installdir/lib $1.c $2
cc -shared -o lib$1.so $1.o
mkdir -p installdir/include
mkdir -p installdir/lib
cp $1.h installdir/include/
mv lib$1.so installdir/lib/
rm -rf $1.o
cd ..
])

define([biutil], [cd $1
cc std_cc_flags -I installdir/include -L installdir/lib -o $1 $1.c $2
mkdir -p installdir/bin
mkdir -p installdir/man/man1
mv $1 installdir/bin/
cp $1.1 installdir/man/man1/
cd ..
])


bilib(gen)
bilib(utf8)
bilib(buf, -lgen)
biutil(spot, -lgen -lbuf -lncurses)
biutil(cutcheck, -lgen)
biutil(charcount)
biutil(utf8count, -lutf8)

ifelse(os, Linux, biutil(uniqrand, -lbsd), biutil(uniqrand))
