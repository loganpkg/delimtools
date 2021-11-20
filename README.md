<!--
Copyright (c) 2021 Logan Ryan McLintock

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

-->
<!--
Description:
toucan is a cross-platform C development environment that contains the
spot text editor and an implementation of the m4 macro processor
-->
toucan
======

History
-------

The toucan is a **production quality** C development environment which will
be **supported indefinitely**.
toucan is the correct repository to access the supported version of the
spot text editor and my implementation of the m4 macro processor.
My GitHub page has older versions of the spot text editor, my
m4 macro processor, freq, and delim, for historical reasons,
but those older versions are no longer supported.

This repository uses sloth version control (not git or fossil) and simply
converts to either git or fossil to be displayed online. See the sloth
repository for more details.

About
-----

toucan is a C development environment that has the following features:

* Cross-platform,
* Self-contained (no explicit linking is required),
* and a Scalable, modular design.

All code and documentation is released under the ISC license.

Applications
------------

The following applications use the toucan environment:

* spot text editor,
* an implementation of the m4 macro processor,
* capybara (a backup utility),
* regr (regular expression find and replace),
* lsha256 (SHA-256 utility),
* freq (character frequency),
* and delim (checks a delimiter).

Modules
-------

toucan has the following modules:

* gen: Generic,
* buf: Buffer,
* gapbuf: Gap buffer,
* minicurses: Curses,
* hashtable: Hash table,
* regex: Regular expression engine,
* fs: File system operations,
* random: Random functions,
* and sha256: SHA-256 algorithm.

Mascot
------

Meet Toucan, this project's mascot, painted by my loving wife Juliana.

![toucan](art/toucan.jpg)

Build
-----

To build the applications, on POSIX-like systems, run:
```
make
```
Otherwise:
```
nmake /f wMakefile.mak
```

Then place the executables somewhere in your PATH.

Enjoy,
Logan =)_
