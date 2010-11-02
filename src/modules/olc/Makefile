################################################################################
# OLC makefile
################################################################################

#
# Enter a list of binary files your module will generate. This should be the
# only variable you will need to modify in this makefile. Everything else
# will handle itself.
#
O_FILES = dedit.o edesc_edit.o exedit.o medit.o oedit.o olc.o redit.o \
	  zedit.o ssedit.o scedit.o resedit.o


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
