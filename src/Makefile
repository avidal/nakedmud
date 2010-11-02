###############################################################################
# Makefile for nakedmud
#
# Many thanks go out to Tyche for suggesting I read "Recursive Make Considered
# Harmful" by Peter Miller. Many of the make conventions used in this makefile
# were taken from Miller's article. The reference is:
# 
# Miller, P.A. (1998). Recursive Make Considered Harmful,
#    AUUGN Journal of AUUG Inc., 19(1), pp. 14-25.
#
###############################################################################

# compiler to use
CC = gcc

# the modules we have installed -- mandatory modules go here
MODULES := char_vars set_val olc2 editor items scripts

# optional modules go on this line
MODULES += time socials alias help


# flags to use during compilation
C_FLAGS := -Wall -g -ggdb -O2

# extra libraries if required
LIBS    := -lz -lpthread -lcrypt


# each module will add to this from its module.mk file
SRC     := gameloop.c mud.c utils.c interpret.c handler.c inform.c movement.c \
	   action.c mccp.c save.c socket.c io.c strings.c event.c \
	   \
	   cmd_comm.c cmd_manip.c cmd_misc.c cmd_admin.c cmd_builder.c \
	   \
	   races.c \
	   \
	   log.c auxiliary.c account_handler.c char_gen.c \
	   \
	   world.c character.c room.c exit.c extra_descs.c object.c body.c \
	   zone.c dialog.c room_reset.c account.c \
	   \
	   list.c property_table.c hashtable.c map.c storage.c set.c \
	   buffer.c bitvector.c


# include the description for each module. These will add to SRC
# C_FLAGS, LIBS, etc...
include $(patsubst %,%/module.mk, $(MODULES))

# determine the object files
O_FILES := $(patsubst %.c,%.o, $(filter %.c, $(SRC)))



################################################################################
# make commands
################################################################################
all: $(O_FILES)
	$(CC) -o NakedMud $(O_FILES) $(LIBS)

# make the object files. The modules are sort of annoying, in that 
# if we do not use -o, the object files will be compiled in this directory,
# and then every time we re-make, the module .o files will be recompiled
# because they are not in their home directory. So... we have to explicitly
# say where the .o files need to go
.c.o: all
	$(CC) -c $(C_FLAGS) -o $(patsubst %.c,%.o, $<) $<

# clear all of the .o files and all of the save files that emacs makes
clean:
	rm -f NakedMud
	rm -f *.o $(patsubst %,%/*.o, $(MODULES))
	rm -f *~ $(patsubst %,%/*~, $(MODULES))

# include all of our dependencies
include $(patsubst %.c,%.d, $(SRC))

# calculate all of our dependencies
%.d: %.c
	./depend.sh $(patsubst %.d,%.c, $@) > $@
