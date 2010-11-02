################################################################################
#
# Python scripting stuff.
#
# To enable python as a scripting language, you have to add a bit of stuff to
# the make process. I run on Mac OSX, so all of the default stuff is set up
# for compiling under OSX, but if you are running on a different OS, you will
# have to change these things. Here is a set of instructions on how to change
# everything to work for you:
#
# STEP 1: Locating Python
#   Figure out where your python directory is. If you do not know how to do
#   this, you can do a "find" from your shell:
#     find / -name "Python.h" -print
#
#   wait for the path to Python.h to show up. You'll have to doctor it a bit;
#   from the path, you will want to figure out the top directory for python, and
#   where all of the header files are located. If the path looks something like:
#     /usr/lib/python2.4/include/Python.h
#
#   PYTHONTOP should be /usr/lib/python2.4
#   and PY_INC should be -I$(PYTHONTOP)/include
#
#
# STEP 2: Getting the linking requirements for python.
#   This is a relatively easy step; start up python (from shell, type "python")
#   and do this:
#
#   >>> import distutils.sysconfig
#   >>> distutils.sysconfig.get_config_var('LINKFORSHARED')
#
#   a string will print that looks something like:
#   '-Xlinker -export-dynamic'
#
#   copy this string (minus the two surrounding ') to the L_PY variable in
#   the Makefile at the src directory (two directories down).
#
#
# STEP 3: Compiling
#   Try compiling. If you get a bunch of errors, go onto step 4.
#
#
# STEP 4: Finding the runtime library.
#   On Mac OSX, you do not need to include the runtime library, but I know
#   for other platforms (fedora, for example) you will need to. If you're on
#   this step, it's probably because you're using one of those other operating
#   systems. You'll need to find the python library and use it during
#   compilation. This isn't too hard. Change your directory to PYTHONTOP and
#   type:
#     find . -name "libpython*a" -print
#
#   when it spits out the address to the python library, set this as the
#   value of PY_LIB in the Makefile at the src directory (two directories down).
#   Now, go back to STEP 3. If you still get errors, you'll want to move onto 
#   STEP 5.
#
# 
# STEP 5: Adding neccessary libraries.
#   Still won't compile? You may need to add some extra libraries that Python
#   requires. These are pretty easy to find, usually; if you are getting
#   getting messages like "undefined references to sin", "undefined reference
#   to pthread_start" etc... you will want to add the libraries that these
#   functions belong to. for each one that comes up, do a "man XXX" and read
#   the man file until you figure out which library the function belongs to.
#   Ones that I found were neccessary for me to include on Fedora were -lm and
#   -ldl . This, of course, will vary from OS to OS. Add all of the libraries
#   you need to the end of L_PY in the Makefile at the src directory (two
#   directories down) and recompile. If things still aren't working for you, 
#   proceed to STEP 6.
#
#
# STEP 6: On your own, or disabling Python
#   If you've gotten to this step, I don't really have much help for you. Your
#   options are a) try to figure the rest out on your own, or disable Python
#   scripts. To disable python scripts, go into mud.h and comment out the
#   MODULE_SCRIPTS define near the top of mud.h
#
#   You will also want to remove all of the script module-related stuff in the 
#   makefile at the src directory (the stuff you added in steps 1-5 and three
#   lines at the very bottom of the makefile). Make clean, and recompile.
#
################################################################################

# compiler to use
CC = gcc

# the top level directory of python
PYTHONTOP = /System/Library/Frameworks/Python.framework/Versions/Current

# the folder where python headers are located
PY_INC    = -I$(PYTHONTOP)/Headers

# Object files we generate
O_FILES = script_set.o script.o pychar.o pyobj.o pymud.o pyroom.o

# the top level directory of the MUD. We need to define this 
# so we can include header files found there
MUDTOP = ../..

#
# compile-time flags
#
C_FLAGS = -Wall -g -ggdb -O2 -I$(MUDTOP) $(PY_INC)



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
