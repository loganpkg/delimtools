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
