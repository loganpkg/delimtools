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

toucan
======

toucan is a cross-platform C module. The idea is, you write code once,
place it in toucan, then in your apps you simple use `#include "use_toucan.h"`.

This repo contains the following apps:
* The spot text editor,
* an implementation of the m4 macro processor,
* regr (regular expression find and replace),
* lsha256 (SHA-256 utility),
* freq (character frequency),
* delim (checks a delimiter),
* and llama (an x86_64 assembler).

The toucan module contains code in the following areas:
* Generic functions,
* Random functions,
* File system operations,
* Buffer,
* Gap buffer,
* Hash table,
* Curses,
* Regular expression engine,
* SHA-256 algorithm.

All code and documentation is released under the ISC license.

Mascot
------

Meet Toucan, this project's mascot, painted by my loving wife Juliana.

![toucan](art/toucan.jpg)

Build
-----

Install on POSIX-like systems:
------------------------------

The default install directory is `"$HOME"/bin`, but this can be changed by
editing the `INSTALL_DIR` variable at the top of the `makefile` file.

Then run:

```
make
make install
make clean
```

Ensure that the install directory is included in your `PATH`.


Other Install:
--------------

The default install directory is `%HOMEPATH%\bin`, but this can be changed by
editing the `INSTALL_DIR` variable at the top of the `wMakefile.mak` file.


Then run:

```
nmake /f wMakefile.mak
nmake /f wMakefile.mak install
nmake /f wMakefile.mak clean
```

Ensure that the install directory is included in your `PATH`.


spot text editor
================

spot is a fast text editor with many advanced features:

* Flicker-free terminal graphics with no external linking to a curses library.
* Can copy and paste between multiple buffers.
* Atomic file saving on POSIX systems.
* Command multiplier prefix.
* Row and column numbers.
* Trim trailing whitespace.
* Region highlighting.
* Regex replace region.
* ANSI C.

The graphics uses the double-buffering method to make smooth screen
changes. The text memory uses the gap buffer method for efficient editing.


To use
------
```
$ spot [file...]
```

Keybindings
-----------

The keybindings can be seen by pressing ESC followed by question mark,
which will display the following:

```
spot keybindings
^ means the control key, and ^[ is equivalent to the escape key.
RK denotes the right key and LK the left key.
Commands with descriptions ending with * take an optional command
multiplier prefix ^U n (where n is a positive number).
^[ ?   Display keybindings in new gap buffer
^b     Backward char (left)*
^f     Forward char (right)*
^p     Previous line (up)*
^n     Next line (down)*
^h     Backspace*
^d     Delete*
^[ f   Forward word*
^[ b   Backward word*
^[ u   Uppercase word*
^[ l   Lowercase word*
^q hh  Quote two digit hexadecimal value*
^a     Start of line (home)
^e     End of line
^[ <   Start of gap buffer
^[ >   End of gap buffer
^[ m   Match bracket
^l     Level cursor and redraw screen
^2     Set the mark
^g     Clear the mark or escape the command line
^x ^x  Switch cursor and mark
^w     Wipe (cut) region
^o     Wipe region appending on the paste gap buffer
^[ w   Soft wipe (copy) region
^[ o   Soft wipe region appending on the paste gap buffer
^k     Kill (cut) to end of line
^[ k   Kill (cut) to start of line
^y     Yank (paste)
^t     Trim trailing whitespace and clean
^s     Forward search
^z     Regex forward search
^[ z   Regex forward search, newline insensitive
^[ n   Repeat the last search type without editing the command line
^r     Regex replace region, where the first character is the delimiter, e.g:
           |find|replace
^[ r   Regex replace region, newline insensitive
^x i   Insert file at cursor
^x ^F  Open file in new gap buffer
^[ =   Rename gap buffer
^x ^s  Save current gap buffer
^x LK  Move left one gap buffer
^x RK  Move right one gap buffer
^[ !   Close current gap buffer without saving
^x ^c  Close editor without saving any gap buffers
```


m4 macro processor
==================

A cross-platform implementation of the m4 macro processor.
It has many nice features:

* Written in ANSI C,
* Has a built-in macro `htdist` that shows the hash table distribution,
* Stack depth only limited by random-access memory (RAM),
* Can be used interactively,
* and implements all of the original m4 built-in commands, except that `syscmd` is
  replaced by `esyscmd` and `eval` is replaced by `add`, `mult`, `sub`, `div`
  and `mod`.

This implementation does not follow the POSIX standards in regards to m4.

To use
------
```
$ m4 [file...]
```

References
----------

* Brian W. Kernighan and Dennis M. Ritchie, The M4 Macro Processor,
  Bell Laboratories, Murray Hill, New Jersey 07974, July 1, 1977.


Mini-tutorial
-------------
These are the built-in macros, presented as a mini-tutorial:
```m4
changequote([, ])
define(cool, $1 and $2)
cool(goat, mice)
undefine([cool])
define(cool, wow)
dumpdef([cool], [y], [define])
hello dnl this will be removed
divnum
divert(2)
divnum
cool
divert(6)
divnum
y
undivert(2)
divert
undivert
incr(76)
len(goat)
index(elephant, ha)
substr(elephant, 2, 4)
translit(bananas, abcs, xyz)
ifdef([cool], yes defined, not defined)
define(y, 5)
ifelse(y, 5, true, false)
dnl By default the esyscmd and maketemp built-in macros are excluded
esyscmd(ifelse(dirsep, /, ls, dir))
esyscmd(echo hello > .test)
include(.test)
maketemp()
errprint(oops there is an error)
htdist
add(8, 2, 4)
mult( , 5, , 3)
sub(80, 20, 5)
div(5, 2)
mod(5, 2)
regexreplace(tiger, ^, big)
regexindex(abcaad, a{2})
```


regex syntax
============

Backslash
---------

A backslash must always be followed by a character. A literal backslash is
entered as two backslashes `\\`.

The following sequences denote special character sets:

* `\w` word set.
* `\W` non-word set.
* `\d` digit set.
* `\D` non-digit set.
* `\s` whitespace set.
* `\S` non-whitespace set.

In all other cases a backslash followed by a character will enter that
character literally. This is useful to remove the special meaning of
a given character, for example, `\.` to search for a literal dot.

The backslash is also used for back-references in the replace component
of the regex. A back-reference gives access to the text from a capture
group. There is an automatic capture group, `\0`, which is the entire
match. Then, as open brackets appear from left to right, there can
be (possibly nested) capture groups of `\1`, `\2`, `\3`, ..., `\9`.

Hooks
-----

* `^` start of line hook. Forces the match to commence at the start of the
  line.
* `$` end of line hook. Forces the match to end at the end of the line.

A `^` character must be at the start of the regex to be considered a hook.
Likewise, a `$` character must be at the end of the regex to be considered
a hook. Otherwise these characters are treated literally (with the exception
that the `^` character denotes negation when it is the first character
in a set).

Capture groups
--------------

The characters `(` and `)` encapsulate a capture group, unless they appear
inside a character set (or are backslashed), in which case they are treated
literally.

Capture groups can be nested and the captured text can be used in the replace
component of the regex via back-references (see the Backslash section above).

Character sets
--------------

The characters `[` and `]` encapsulate a character set. A character set is
treated as an atom in the compiled regex atom-chain. A standalone
non-special character is also an atom.

If the first character after the opening `[` square bracket is a `^`
then the character set will be negated. A `^` at any other position in the set
will be treated literally.

Character sets can contain ranges. Ranges are of the form of two characters
separated by a hyphen, such as `A-Z` or `0-9`, etc. All ASCII values between
the two characters (including the start character and the end character)
will be added to the set.

Special character sets, such as `\w` can be used inside a `[` `]` standard
character set.

Multiplier
----------

An atom can be followed by a multiplier which applies bounds to how many times
the preceding atom can be repeatedly matched.

* `*` match zero or more times.
* `+` match one or more times.
* `?` match zero or one times.
* `{n}` match exactly `n` times.
* `{,n}` match zero to `n` times.
* `{m,}` match `m` or more times.
* `{m, n}` match between `m` to `n` times (inclusive of `m` and `n`).

Dot
---

The `.` character denotes a special character set which includes all possible
characters.

Greedy
------

This engine will look for the left-most longest possible match.



Enjoy,
Logan =)_
