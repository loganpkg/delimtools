# loganutils

loganutils is a collection of my software. It includes the
spot text editor and various command line utilities.

Each subdirectory is its own project. The projects
share the same repository to make it easy to share code
between them.

All software is released under the ISC licence (OpenBSD style).

## Installation

    $ git clone https://github.com/loganpkg/loganutils.git

Set `installdir` in the `buildall.sh` file, the default is
the `$HOME` directory.

    $ m4 buildall.sh | sh


## buf

buf is a gap buffer library.

It has many advanced features:
* buffer sizes and number of buffers only limited by random access memory,
* efficient file reading and writing,
  and efficient killregion and yank functions,
* automatic row number updating,
* intelligent gap increases,
* and integer overflow protection.

## spot

spot is a minimalistic text editor that uses the gen, buf and ncurses
libraries.

It has many advanced features:
* region highlighting,
* full editing available in the command line,
* and gap buffer independent graphics.

## gen

gen is a library of generic C code.

## utf8

utf8 is a library of UTF-8 code.

## cutcheck

The `cutcheck` utility checks that the delimiter appears
the same number of times in each line.

## charcount

The `charcount` utility counts chars, and shows the count
for each char value encountered.

## utf8count

The `charcount` utility counts UTF-8 characters, and shows the count
for each code point value encountered.

## uniqrand

The `uniqrand` utility generates a given number of unique uniformly
distributed random numbers between a lower and upper bound.

## possum

The `possum` utility organises photos and movies into a year
and month directory structure based on the creation date stored
inside the file. It also removes duplicate files using `jdupes`.
This requires the `exiftool` to be installed.
