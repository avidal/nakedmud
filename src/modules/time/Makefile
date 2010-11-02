###############################################################################
# Makefile for time
###############################################################################

# compiler to use
CC = gcc


################################################################################
#
# Object files that this module generates. This should be the only only thing
# you need to modify!!!
#
################################################################################
O_FILES = time.o




# the top level directory of the MUD. We need to define this 
# so we can include header files found there
MUDTOP = ../..


################################################################################
#
# Libraries and compile-time flags
#
################################################################################
# flags to use during compilation
C_FLAGS = -Wall -g -ggdb -O2 -I$(MUDTOP)


################################################################################
# make commands
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
