##
# Void editor
#
# @file
# @version 0.1

voided: voided.c
	$(CC) voided.c -o voided -Wall -Wextra -pedantic -std=c99

# end
