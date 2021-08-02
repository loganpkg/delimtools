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
^s     Search
^[ n   Search without editing the command line
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

Enjoy,
Logan =)_
