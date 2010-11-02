################################################################################
#
# demo.py
#
# This is a demo module that gives the basics on how to extend the mud via
# Python modules. Python is primarily used for scripting in NakedMud, but it is
# only a couple small steps to integrate python and the mud enough to let Python
# add commands, events, actions, and entirely new systems like combat and magic.
# 
# There are pros and cons that come with extending the mud with Python, and
# extending the mud with C. Python is a much easier language to program in than
# C, especially when it comes to prototyping new ideas; programming an idea in
# Python will often take much less time than it will in C. Python code also
# tends to be much shorter than C code, and is thus easier to maintain. Another
# beautiful feature of the way Python is integrated into the MUD is that
# problems in the code will rarely crash the mud. Rather, they'll just throw
# an error that we can display, and continue working fine.
#
# The downside is that Python has extremely limited access to the C code that
# serves as the foundation for NakedMud. Python can't  call any player commands
# that are written in C (yet), Python can't add auxiliary data to the different
# types of data in the mud (yet), and Python doesn't have the capabilities to
# interact with some of the important C modules and systems (e.g. set_val, 
# olc, races, bodyparts, character generation, storage sets). These are all 
# things that will come in time, but at the moment, extending the mud in Python
# means giving away the ability to do many things that will eventually be 
# important.
#
# You won't be able to do everything with python modules, but you WILL be able
# to do a hell of a lot... 
#
################################################################################
from mud import add_cmd

#
# Our first character command added with Python! Like a normal MUD command,
# this command takes in 4 arguments: the character who performed it, the command
# name, a subcommand value, and the argument supplied to the command. ch is
# a Character, cmd is a string, subcmd is an int, and arg is a string
def cmd_pycmd(ch, cmd, subcmd, arg):
    ch.send("Hello, " + ch.name + ". This is a demo Python command!")

# let the MUD know it should add a command. This works exactly like add_cmd in 
# the C source. Good examples of how to use it can be found in interpret.c
add_cmd('pycmd', None, cmd_pycmd, 0, 'unconcious', 'flying', 'admin', 
	False, False)
