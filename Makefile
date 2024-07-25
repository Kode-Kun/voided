##
# Void editor
#
# compile command: time make && echo $?
# @file
# @version 0.1
CC=gcc
CFLAGS=-Wall -Wextra -pedantic -std=c99
TARGET=voided
SRC=voided.c
INSTDIR=/usr/local/bin/

default:
	${CC} ${SRC} -o ${TARGET} ${CFLAGS}

clean:
	rm ${TARGET}

install:
	cp ${TARGET} ${INSTDIR}${TARGET}
# end
