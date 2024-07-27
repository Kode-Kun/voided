# Void Editor

`voided` is a simple, modal, terminal-based text editor written in C using [kilo](https://github.com/antirez/kilo) as a base. 
It has no dependencies except glibc, all rendering is done through VT100 escape sequences.
All you need is a C compiler.

`voided` is still in very early development. There are plans for [suckless](https://suckless.org/)-style patches to add optional functionality in the future, once a stable API is reached.  

# Building

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

