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
INSTALL_PREFIX = %HOMEPATH%\bin

F = /Ot

all: spot.exe m4.exe regr.exe lsha256.exe freq.exe delim.exe

spot.exe: toucan.obj spot.c
        cl $(F) spot.c toucan.obj

m4.exe: toucan.obj m4.c
        cl $(F) m4.c toucan.obj

regr.exe: toucan.obj regr.c
        cl $(F) regr.c toucan.obj

lsha256.exe: toucan.obj lsha256.c
        cl $(F) lsha256.c toucan.obj

freq.exe: toucan.obj freq.c
        cl $(f) freq.c toucan.obj

delim.exe: toucan.obj delim.c
        cl $(F) delim.c toucan.obj

llama.exe: toucan.obj llama.c
        cl $(F) llama.c toucan.obj

toucan.obj: use_toucan.h toucan.h toucan.c
        cl /c $(F) toucan.c

install: copy_files

copy_files: establish_dir
        copy /Y *.exe $(INSTALL_DIR)

establish_dir:
        if not exist $(INSTALL_DIR) mkdir $(INSTALL_DIR)

clean:
        del *.exe *.obj
