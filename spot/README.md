
    Copyright (c) 2019 Logan Ryan McLintock

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.


# spot

spot is a minimalistic text editor that uses the gap buffer method and the
ncurses library.

Please see the man page for documentation. This README only
contains installation and printing information.

## Installation

    $ git clone https://github.com/loganpkg/spot.git
    $ cd spot
    $ make
    $ PREFIX="$HOME" make install

For the `sh` shell, add the following lines to `~/.shrc` as necessary:

    # Set this to the PREFIX that you used
    install_dir=/usr/local
    # If the executable is not found
    export PATH="$install_dir"/bin:"$PATH"
    # If the manual page is not found
    export MANPATH="$install_dir"/man:"$(manpath)"

then reload the configuration file:

    $ . ~/.shrc

## Printing

There is a `LaTeX` file `print.tex` that you can use to create
a pdf of the whole project. This is useful for printing.

Firstly, install `spot` as per the instructions above.
Then make sure you have `LaTeX` and `ps2pdf` installed.

The following commands will build the pdf:

    $ man -t spot > spot.ps
    $ ps2pdf spot.ps
    $ pdflatex print.tex
    $ pdflatex print.tex

The output will be in the file `print.pdf`.

Enjoy,
Logan