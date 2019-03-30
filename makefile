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

# Set this
install_dir = /usr/local

CC = cc
CFLAGS = -ansi -Ofast -Wall -Wextra -pedantic
RM = rm -f

include_path = -I$(install_dir)/include
link_path = -L$(install_dir)/lib
libs = -lutf8
cp = cp
mkdir = mkdir -p

all: delim delf freq

delim: delim.c
	$(CC) $(CFLAGS) $(include_path) -o delim delim.c

delf: delf.c
	$(CC) $(CFLAGS) $(include_path) $(link_path) -o delf delf.c $(libs)

freq: freq.c
	$(CC) $(CFLAGS) $(include_path) $(link_path) -o freq freq.c $(libs)

.PHONY: clean
clean:
	$(RM) delim delf freq

.PHONY: install
install:
	$(mkdir) $(install_dir)/bin
	$(mkdir) $(install_dir)/man/man1
	$(cp) delim $(install_dir)/bin/
	$(cp) delf $(install_dir)/bin/	
	$(cp) freq $(install_dir)/bin/
	$(cp) delim.1 $(install_dir)/man/man1/
	$(cp) delf.1 $(install_dir)/man/man1/
	$(cp) freq.1 $(install_dir)/man/man1/
