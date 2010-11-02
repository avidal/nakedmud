################################################################################
#
# inform.py
#
# Python's mirror of C's inform.c -- contains various functions that perform
# informative duties. Examining places/things, displaying proper names, etc...
#
################################################################################
from mud import *
import utils, char, hooks



################################################################################
# utility functions
################################################################################
def see_char_as(ch, tgt):
    '''returns the proper name one character sees another by'''
    if ch.cansee(tgt):
        return tgt.name
    else:
        return "someone"

def see_obj_as(ch, obj):
    '''returns the proper name a character sees an object by'''
    if ch.cansee(obj):
        return obj.name
    else:
        return "something"

def see_exit_as(ch, ex):
    '''returns the proper name a character sees an exit by'''
    if ch.cansee(ex):
        return ex.name
    else:
        return "something"

def show_equipment(ch, tgt):
    '''shows ch tgt\'s equipment'''
    for part in tgt.bodyparts:
        obj = tgt.get_equip(part)

        # if it's not there, or on someone else and we can't see it, skip it
        if obj == None or (ch != tgt and not ch.cansee(obj)):
            continue

        ch.send("%-30s %s" % ("{c<{C" + part + "{c>{n", see_obj_as(ch, obj)))

def build_who():
    '''returns a formatted list of all the people currently online'''
    buf = "--------------------------------------------------------------------------------\r\n"

    # build character info
    count = len(char.socket_list())
    for ch in char.socket_list():
        buf = buf+(" %-16s %-15s %45s "%(ch.name,ch.race,ch.user_groups))+"\r\n"

    conn_end = "s"
    if count == 1: conn_end = ""

    # build our footer
    buf = buf + "--------------------------------------------------------------------------------\r\n" + (" %d player" % count)  + conn_end + " currently logged in.\r\n" + "--------------------------------------------------------------------------------\r\n"
    
    return buf



################################################################################
# hooks
################################################################################
def equipment_look_hook(info):
    '''displays a character\'s equipment when looked at'''
    tgt, ch = hooks.parse_info(info)

    if ch != tgt:
        gndr = tgt.heshe
        act  = "is"
    else:
        gndr = "You"
        act  = "are"

    ch.send("\n" + gndr + " " + act + " wearing:")
    show_equipment(ch, tgt)



################################################################################
# add our hooks
################################################################################
hooks.add("look_at_char", equipment_look_hook)



################################################################################
# unload procedure
################################################################################
def __unload__():
    '''things that need to be detached when the module is un/reloaded'''
    hooks.remove("look_at_char", equipment_look_hook)
