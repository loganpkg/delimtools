
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


# uniqrand

uniqrand generates unique uniformly distributed random numbers
in a random order.

uniqrand is released under the ISC licence (OpenBSD style).

Please see the man page for documentation. This README only
contains installation information.

## Installation

Clone and build. You need to use BSD make, not GNU make.
Set `PREFIX` to your installation directory:

    $ git clone https://github.com/loganpkg/uniqrand.git
    $ cd uniqrand
    $ make
    $ PREFIX="$HOME" make install


Enjoy,
Logan
