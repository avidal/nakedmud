################################################################################
#
# Generic Makefile
#
# The intent of this makefile is to act as a template makefile for people
# creating new modules for NakedMud(tm). The module installation process has
# been made as simple as I (Geoff Hollis) can make it; for the make file, all
# you have to do is set O_FILES to a list of object files your module will
# generate. For guidelines and instructions on installing new modules into
# NakeMud(tm), please read the MODULES.txt file in the doc folder, located at
# the root of the NakedMud directory.
#
# Questions and comments should be directed to Geoff Hollis (hollis@ualberta.ca)
#
################################################################################

#
# Enter a list of binary files your module will generate. This should be the
# only variable you will need to modify in this makefile. Everything else
# will handle itself.
#
O_FILES = 


# compiler to use
CC = gcc


# the top level directory of the MUD. We need to define this 
# so we can include header files found there
MUDTOP = ../..


#
# compile-time flags
#
C_FLAGS = -Wall -g -ggdb -O2 -I$(MUDTOP)




################################################################################
#
# make commands
#
################################################################################
all: $(O_FILES)
	cp *.o $(MUDTOP)

# make the object files
.c.o: all
	$(CC) -c $(C_FLAGS) $<

# make dependancies for all of the files
depend:
	$(CC) -MM *.c > .depend

-include .depend

clean:
	rm -f *.o
	rm -f *.*~
