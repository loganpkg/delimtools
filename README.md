
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


# delimtools

delimtools is a collection of utilities for working with delimited files.

## History

delimtools is unsupported and just presented for historical reasons.

## Installation

Create a directory for the source code:

    $ cd ~
    $ mkdir delimtools_src
    $ cd delimtools_src

Download the source code from the offical fossil repository:

    $ fossil clone https://chiselapp.com/user/logan/repository/delimtools repo_delimtools
    $ fossil open repo_delimtools

If you don't have `fossil` then you can mannually download the files
into `delimtools_src`.

Now it's time to build the software:

    $ make
    $ PREFIX="$HOME" make install

You might need to use `sudo` for the `make install` step depending on your choice of `PREFIX`.

Some paths might need to be added to your shell's configuration file.
Depending on your shell and operating system, the following instructions will be
slightly different.

Add the following lines to `~/.shrc` as necessary:

    # Set this to the PREFIX that you used
    install_dir="$HOME"
    # If the executable is not found
    export PATH="$install_dir"/bin:"$PATH"
    # If the shared object is not found
    export LD_LIBRARY_PATH="$install_dir"/lib:"$LD_LIBRARY_PATH"
    # If the manual page is not found
    export MANPATH="$install_dir"/man:"$(manpath)"

then reload the configuration file:

    $ . ~/.shrc

You can check that the correct utf8 shared object is being used:

    $ ldd "$(which freq)"

## Printing

There is a `LaTeX` file `print.tex` that you can use to create a
combined pdf of the whole project. This is useful for printing.

Firstly, install `delimtools` as per the instructions
above. Then make sure you have `LaTeX` and `ps2pdf` installed.

The following commands will build the pdf:

    $ man -t delim > delim.ps
    $ ps2pdf delim.ps
    $ man -t delf > delf.ps
    $ ps2pdf delf.ps
    $ man -t freq > freq.ps
    $ ps2pdf freq.ps
    $ pdflatex print.tex
    $ pdflatex print.tex

The output will be in the file `print.pdf`.

Enjoy,
Logan
