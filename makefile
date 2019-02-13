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

CFLAGS = -ansi -Ofast -Wall -Wextra -pedantic
RM = rm -f

mkdir = mkdir -p
cp = cp -p
man_dir = $(HOME)/man/man1

all: delim delf freq
delim: delim.c
delf: delf.c
freq: freq.c


.PHONY: clean
clean:
	$(RM) delim delf freq

.PHONY: install
install:
	$(mkdir) $(HOME)/bin
	$(mkdir) $(man_dir)
	$(cp) delim $(HOME)/bin/
	$(cp) delf $(HOME)/bin/
	$(cp) freq $(HOME)/bin/
	$(cp) delim.1 $(man_dir)/
	$(cp) delf.1 $(man_dir)/	
	$(cp) freq.1 $(man_dir)/
