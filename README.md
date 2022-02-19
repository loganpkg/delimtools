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
spot text editor
================

spot is a fast text editor with many advanced features:

* Flicker-free built-in terminal graphics, with no curses library need!
* Can copy and paste between multiple buffers.
* Command multiplier prefix.
* Row and column numbers.
* Trim trailing whitespace.
* Region highlighting.
* Regex replace region.
* ANSI C except for some graphics related code.
* Cross-platform with minimal #ifdef usage.

The graphics uses the double-buffering method to make smooth screen
changes. The text memory uses the gap buffer method for efficient editing.

Mascot
------

Meet Toucan, this project's mascot, painted by my loving wife Juliana.

![toucan](art/toucan.jpg)

To install
----------

```
cc spot.c && mv a.out spot
```
or
```
cl spot
```

And place the executable somewhere in your `PATH`.


To use
------
```
spot [file...]
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

Hooks (anchors)
---------------

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
