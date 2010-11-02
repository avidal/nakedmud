################################################################################
#
# cmd_admin.py
#
# commands available only to admins.
#
################################################################################
from mud import *
from mudsys import add_cmd
import mudsys, inform, char, string



def cmd_shutdown(ch, cmd, arg):
    '''boom! shut the mud down'''
    mudsys.do_shutdown()

def cmd_shutdown_net(ch, cmd, arg):
    '''a trap to make sure we spell shutdown out completely'''
    ch.send("You must spell out shutdown completely!")

def cmd_copyover(ch, cmd, arg):
    '''restart the mud, but keep all sockets attached'''
    mudsys.do_copyover()

def cmd_copyover_net(ch, cmd, arg):
    '''a trap to make sure we spell copyover out completely'''
    ch.send("You must spell out copyover completely!")

def cmd_repeat(ch, cmd, arg):
    '''performs the same command multiple times'''
    try:
        times, arg = parse_args(ch, True, cmd, arg, "int string")
    except: return

    if times < 1:
        ch.send("Commands may only be repeated a positive number of times.")
    else:
        for i in range(times):
            ch.act(arg, True)

def cmd_pulserate(ch, cmd, arg):
    '''changes the number of pulses the mud experiences each second'''
    if arg == '':
        ch.send("The mud currently has "+mudsys.sys_getval("pulses_per_second")+
                "pulses per second.")
    else:
        pulserate = string.atoi(arg)
        if pulserate == 0 or 1000 % pulse != 0:
            ch.send("The number of pulses per second must divide 1000.")
        else:
            mudsys.sys_setval("pulses_per_second", str(pulserate))
            ch.send("The mud's new pulse rate is %d pulses per second." %
                    pulserate)

def cmd_lockdown(ch, cmd, arg):
    '''Locks the game for anyone not a member of one of the user groups
       we specify.'''
    if arg == '':
        lockdown = mudsys.sys_getval("lockdown")
        if lockdown == '':
            ch.send("Lockdown is currently off.")
        else:
            ch.send("Lockdown is currently to members not of: " + lockdown)
            ch.send("To turn off lockdown, use {clockdown off{n")

    elif arg.lower() == "off":
        ch.send("Lockdown disabled.")
        mudsys.sys_setval("lockdown", "")

    # make sure we're not locking ourself out
    elif not ch.isInGroup(arg):
        ch.send("You cannot lock yourself out!")

    else:
        ch.send("MUD locked down to everyone not in groups: " + arg)
        mudsys.sys_setval("lockdown", arg)

        # kick out everyone who we've just locked out
        for ch in char.char_list():
            if ch.is_pc and not ch.isInGroup(arg):
                ch.send("The mud has just been locked down to you.")
                mudsys.do_save(ch)
                mudsys.do_disconnect(ch)
                extract(ch)

def cmd_at(ch, cmd, arg):
    '''Perform a command at another room or person'''
    try:
        found, type, arg = parse_args(ch, True, cmd, arg,
                                      "{ room ch.world.noself } string")
    except: return

    # figure out what room we're doing the command at
    if type == "char":
        room = found.room
    else:
        room = found

    # transfer us over to the new room, do the command, then transfer back
    old_room = ch.room
    ch.room  = room
    ch.act(arg, True)
    ch.room  = old_room

def try_force(ch, vict, cmd):
    '''tries to force a person to do something'''
    if ch == vict:
        ch.send("Why don't you just try doing it?")
    elif vict.isInGroup("admin"):
        ch.send("But " + vict.name + " has just as many priviledges as you!")
    else:
        ch.send("You force " + vict.name + " to '" + cmd + "'")
        vict.send(inform.see_char_as(vict, ch) + " forces you to '" + cmd + "'")
        vict.act(cmd, False)

def cmd_force(ch, cmd, arg):
    '''force someone to execute a command'''
    try:
        found, multi, arg = parse_args(ch, True, cmd, arg,
                                       "ch.world.noself.multiple string")
    except: return

    if multi == False:
        try_force(ch, found, arg)
    else:
        for vict in found:
            try_force(ch, vict, arg)

def cmd_goto(ch, cmd, arg):
    '''Go to a specific room, object, or character in the game. Rooms are
       referenced by vnum. Everything else is referenced by name.
       usage: goto <thing>
       
       examples:
         goto room@zone       go to room in zone
         goto jim             go to an object/person named jim'''
    try:
        found, type = parse_args(ch, True, cmd, arg, "{ room ch.world.noself }")
    except: return

    # what did we find?
    if type == "char":
        dest = found.room
    else:
        dest = found

    message(ch, None, None, None, True, "to_room",
            "$n disappears in a puff of smoke.")
    ch.room = dest
    ch.act("look")
    message(ch, None, None, None, True, "to_room",
            "$n appears in a puff of smoke.")

def do_transfer(ch, tgt, dest):
    '''ch transfers tgt to dest'''
    if tgt.room == dest:
        ch.send(tgt.name + " is already there")
    else:
        tgt.send(inform.see_char_as(tgt, ch) + " has transferred you to " +
                 dest.name)
        message(tgt, None, None, None, True, "to_room",
                "$n disappears in a puff of smoke.")
        tgt.room = dest
        tgt.act("look", False)
        message(tgt, None, None, None, True, "to_room",
                "$n arrives in a puff of smoke.")

def cmd_transfer(ch, cmd, arg):
    '''The opposite of goto. Instead of moving to a specified location, it
       takes the target to the user.
       usage: transfer <player> [[to] room]'''
    try:
        found, multi, dest = parse_args(ch, True, cmd, arg,
                                        "ch.world.multiple.noself | [to] room")
    except: return

    # if we didn't supply a room, use our own
    if dest == None:
        dest = ch.room

    # do our transfers
    if multi == False:
        do_transfer(ch, found, dest)
    else:
        for tgt in found:
            do_transfer(ch, tgt, dest)



################################################################################
# add our commands
################################################################################
add_cmd("shutdow",  None, cmd_shutdown_net, "admin", False)
add_cmd("shutdown", None, cmd_shutdown,     "admin", False)
add_cmd("copyove",  None, cmd_copyover_net, "admin", False)
add_cmd("copyover", None, cmd_copyover,     "admin", False)
add_cmd("at",       None, cmd_at,           "admin", False)
add_cmd("lockdown", None, cmd_lockdown,     "admin", False)
add_cmd("pulserate",None, cmd_pulserate,    "admin", False)
add_cmd("repeat",   None, cmd_repeat,       "admin", False)
add_cmd("force",    None, cmd_force,        "admin", False)
add_cmd("goto",     None, cmd_goto,         "admin", False)
add_cmd("transfer", None, cmd_transfer,     "admin", False)
