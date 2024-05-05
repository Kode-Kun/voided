##
# Void editor
#
# compile command: time make && ./voided && echo $?
# @file
# @version 0.1
CC=gcc
CFLAGS=-Wall -Wextra -pedantic -std=c99
TARGET=voided
SRC=voided.c


voided: voided.c
	${CC} ${SRC} -o ${TARGET} ${CFLAGS}
# end
