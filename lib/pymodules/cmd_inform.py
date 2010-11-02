################################################################################
#
# cmd_inform.py
#
# Contains various commands that are informative in nature. For instance, look,
# equipment, inventory, etc...
#
################################################################################
from mud import *
from inform import *
from mudsys import add_cmd
import utils



################################################################################
# utility functions
################################################################################
def cmd_inventory(ch, cmd, arg):
    '''displays the character inventory to himself'''
    if len(ch.inv) == 0:
        ch.send("You are not carrying anything.")
    else:
        ch.send("{gYou are carrying:")
        visible = utils.find_all_objs(ch, ch.inv, "", None, True)
        utils.show_list(ch, visible, lambda(x): x.name, lambda(x): x.mname)


def cmd_equipment(ch, cmd, arg):
    '''displays a character\'s equipment to himself'''
    ch.send("You are wearing:")
    show_equipment(ch, ch)

def cmd_who(ch, cmd, arg):
    '''lists who is all online to the character'''
    ch.page(build_who())
    
def cmd_look(ch, cmd, arg):
    '''allows players to examine just about anything in the game'''
    if arg == '':
        inform.look_at_room(ch, ch.room)
    else:
        found, type = generic_find(ch, arg, "all", "immediate", False)

        # what did we find?
        if found == None:
            ch.send("What did you want to look at?")
        elif type == "obj" or type == "in":
            inform.look_at_obj(ch, found)
        elif type == "char":
            inform.look_at_char(ch, found)
        elif type == "exit":
            inform.look_at_exit(ch, found)

        # extra descriptions as well
        ############
        # FINISH ME
        ############



################################################################################
# add our commands
################################################################################
add_cmd("inventory", "inv", cmd_inventory, "sitting", "flying",
        "player", True, False)
add_cmd("equipment", "eq",  cmd_equipment, "sitting", "flying",
        "player", True, False)
add_cmd("worn",      None,  cmd_equipment, "sitting", "flying",
        "player", True, False)
add_cmd("who",       None,  cmd_who,       "sitting", "flying",
        "player", True, False)

'''
add_cmd("look",      "l",   cmd_look,      "sitting", "flying",
        "player", True, False)
'''
