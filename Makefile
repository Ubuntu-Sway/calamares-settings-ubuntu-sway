#!/usr/bin/make -f

#SUBDIRS :=

all:
	# Get rid of the unnecessary files
	find ubuntusway/ -type f -iname "*.in" | xargs rm -f
	find ubuntusway/ -type f -iname "Makefile" | xargs rm -f

# vim:ts=4
