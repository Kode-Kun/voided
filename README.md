# Void Editor

`voided` is a simple, modal, terminal-based text editor written in C using [kilo](https://github.com/antirez/kilo) as a base. 
It has no dependencies except glibc, all rendering is done through VT100 escape sequences.
All you need to build it is a C compiler.

`voided` is in early development. There are still a few core features that have yet to be added, such as:

+ a `--VISUAL--` mode, for selecting text with the cursor
+ copying and pasting (not possible until visual mode is implemented)
+ a simple search feature
+ syntax highlighting
+ easier and more approachable configuration with the help of a config file

There are also plans for [suckless](https://suckless.org/)-style patches to add optional functionality in the future, once a stable API is reached.

## Building

The easiest way to build `voided` is with `make`, although it is by no means obligatory.

On Linux, simply run:
```sh
$ make
```
then
```sh
$ sudo make install
```

This will compile `voided` and copy it into your `/usr/local/bin/` directory. If you wish to put it in a different directory, skip the last step.
