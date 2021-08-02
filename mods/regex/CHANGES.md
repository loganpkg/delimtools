CHANGES
=======

I (Logan Ryan McLintock) have made trivial modifications to code
in this directory. I do not consider these changes a derivative work
and hence I do not claim copyright for them. In the event that they
are considered a derivative work, I dedicate my copyright interests
in this software to the public domain.

List of modifications made by Logan Ryan McLintock:

* Download OpenBSD source from https://github.com/openbsd/src
  (on 25th of July 2021).

* Add in `regex.h` from `src/include/regex.h`.

* Rename `Makefile.inc` to `Makefile.inc_do_not_use`.

* Include gen header in `utils.h`.

* Comment out `DEF_WEAK(regfree);` from `regfree.c`.

* Change `inline` functions in `regex2.h` to macros in order to use ANSI C.

* Change some `int i` to `size_t i` in `regcomp.c` and `engine.c` to stop
  compiler comparison warning.

* Comment out `__BEGIN_DECLS` and `__END_DECLS` in `regex.h`.

* Change `<regex.h>` to `"regex.h"` in `regcomp.c`, `regerror.c`, `regexec.c`,
  and `regfree.c`.

* Set `DUPMAX` to `255` in `utils.h`.

* Define `_XOPEN_SOURCE` as `500` in `regerror.c` to use `snprintf`.

* Initialise `start` and `newstart` to `NULL` in `regcomp.c` to stop compiler
  warning.

* Add casts to `void` to stop compiler warinings after `assert` calls in
  `engine.c`.

* Cast `CHAR_BIT*sizeof(states1)` to `sopno` to stop compiler warning.

* Initialise `prevback` and `prevfwd` to stop compiler warnings in `regcomp.c`.

* Comment out define in `utils.h` to set NDEBUG externally.
