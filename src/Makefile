###############################################################################
#Makefile for nakedmud
###############################################################################

# compiler to use
CC = gcc


################################################################################
#
# Libraries and compile-time flags
#
################################################################################

# flags to use during compilation
C_FLAGS = -Wall -g -ggdb -O2

# libraries
L_FLAGS = -lz -lpthread

# linking requirements for the python scripting module. See
# src/modules/scripts/makefile
L_PY     = -u __dummy -u _PyMac_Error -framework Python -framework System \
	   -framework CoreServices -framework Foundation

# the location of the Python .a file, if it is needed. See 
# src/modules/scripts/Makefile
PY_LIB   =


################################################################################
#
# object files, and source files for python extentions
#
################################################################################
O_FILES = gameloop.o mud.o utils.o interpret.o handler.o inform.o movement.o \
	  action.o mccp.o save.o socket.o io.o strings.o event.o \
	  \
	  cmd_comm.o cmd_manip.o cmd_misc.o \
	  \
	  text_editor.o builder.o admin.o \
	  \
	  items.o races.o log.o help.o auxiliary.o \
	  \
	  world.o character.o room.o exit.o extra_descs.o object.o body.o \
	  zone.o dialog.o room_reset.o \
	  \
	  list.o property_table.o hashtable.o hashmap.o storage.o



################################################################################
#
# make commands
#
################################################################################
all: $(O_FILES)
	make module
	rm -f NakedMud
	$(CC) -o NakedMud *.o $(PY_LIB) $(L_FLAGS) $(L_PY)

# make the object files
.c.o: all
	$(CC) -c $(C_FLAGS) $<

# make dependancies for all of the files
depend:
	$(CC) -MM *.c > .depend
	make moduledepend

clean:
	rm -f *.o
	rm -f *.o
	make moduleclean
	rm -f NakedMud
	rm -f *.*~


include .depend


################################################################################
#
# when installing new modules, you will need to add the name of your module
# to the MODULES list
#
################################################################################
MODULES = time olc scripts

# compile all of the modules we have installed
module:
	for dir in $(MODULES); do             \
	  (cd modules/$$dir; $(MAKE) all);    \
	done

# clean all of our modules out
moduleclean:
	for dir in $(MODULES); do             \
	  (cd modules/$$dir; $(MAKE) clean);  \
	done

# make dependancies for all of our modules
moduledepend:
	for dir in $(MODULES); do             \
	  (cd modules/$$dir; $(MAKE) depend); \
	done
