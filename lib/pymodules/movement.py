################################################################################
#
# movement.py
#
# all of the functions concerned with movement and position change
#
################################################################################
from mud import *
from mudsys import add_cmd
import inform, hooks



# a ranking of positions
positions     = ["unconscious", "sleeping", "sitting", "standing", "flying"]
pos_act       = ["collapse",   "sleep",    "sit",     "stand",    "fly"]

# stuff for handling movement
dir_name = ["north", "east", "south", "west", "northeast", "northwest",
                 "southwest", "southeast", "up", "down"]
dir_abbr = ["n", "e", "s", "w", "ne", "nw", "sw", "se", "u", "d"]
dir_opp  = [  2,   3,   0,   1,   6,     7,    4,    5,   9,   8]



def try_use_furniture(ch, obj, pos):
    '''attempts to resituate a person on the piece of furniture'''
    # are we already on it?
    if ch.on == obj:
        ch.send("You are already " +ch.on.furniture_type+ " " +ch.on.name+ ".")
    # make sure we have room
    elif obj.furniture_capacity <= len(obj.chars):
        ch.send("There isn't any room left.")
    else:
        # are we already on something? get up first
        if ch.on:
            message(ch,None,ch.on,None,True,"to_char","You stand up from $o.")
            message(ch,None,ch.on,None,True,"to_room","$n stands up from $o.")
            ch.on = None

        # send our messages for sitting down
        act = pos_act[positions.index(pos)]
        message(ch, None, obj, None, True,
                "to_char", "You " + act + " " + obj.furniture_type + " $o.")
        message(ch, None, obj, None, True,
                "to_room", "$n " + act + " " + obj.furniture_type + " $o.")

        # place ourselves down on our new furniture
        ch.on  = obj
        ch.pos = pos
        return True

    # we didn't manage to get on the furniture
    return False

def try_change_pos(ch, pos):
    '''this function attempts to change the position of the person'''
    if ch.pos == pos:
        ch.send("You are already " + pos + ".")
        return False
    else:
        if ch.pos == "flying":
            message(ch, None, None, None, True, "to_char", "You stop flying.")
            message(ch, None, None, None, True, "to_room", "$n stops flying.")

        act = pos_act[positions.index(pos)]
        message(ch, None, None, None, True, "to_char", "You " + act + ".")
        message(ch, None, None, None, True, "to_room", "$n "  + act + "s.")
        ch.pos = pos
        return True

def cmd_sit(ch, cmd, arg):
    '''attempts to sit'''
    try:
        obj, = parse_args(ch, True, cmd, arg, "| [on] obj.room")
    except: return

    if obj == None:
        try_change_pos(ch, "sitting")
    elif obj.istype("furniture"):
        try_use_furniture(ch, obj, "sitting")
    else:
        ch.send("You cannot sit on " + inform.see_obj_as(ch, obj) + ".")

def cmd_sleep(ch, cmd, arg):
    '''attempts to sleep'''
    try:
        obj, = parse_args(ch, True, cmd, arg, "| [on] obj.room")
    except: return

    if obj == None:
        try_change_pos(ch, "sleeping")
    elif obj.istype("furniture"):
        try_use_furniture(ch, obj, "sleeping")
    else:
        ch.send("You cannot sleep on " + inform.see_obj_as(ch, obj) + ".")

def cmd_stand(ch, cmd, arg):
    '''attempts to stand'''
    try_change_pos(ch, "standing")

def cmd_wake(ch, cmd, arg):
    '''attempts to wake up'''
    message(ch,None,None,None,True, "to_char", "You stop sleeping and sit up.")
    message(ch,None,None,None,True, "to_room", "$n stops sleeping and sits up.")
    ch.pos = "sitting"

def dir_index(dir):
    '''returns the index of the direction name'''
    try:
        return dir_name.index(dir)
    except: pass
    try:
        return dir_abbr.index(dir)
    except: pass
    return -1

def try_move_mssg(ch, dir):
    '''Handles all moving of characters from one room to another, through
       commands. Attempts a move. If successful, returns the exit left through.
       Informs people of our moving'''
    old_room = ch.room
    ex       = try_move(ch, dir)
    dirnum   = dir_index(dir)

    # did we successfully move?
    if ex != None:
        new_room = ch.room
        ch.room  = old_room

        # send out our leave messages as needed
        if ex.leave_mssg != '':
            message(ch, None, None, None, True, "to_room", ex.leave_mssg)
        elif dirnum == -1:
            message(ch, None, None, None, True, "to_room", "$n leaves.")
        else:
            message(ch, None, None, None, True, "to_room",
                    "$n leaves " + dir_name[dirnum] + ".")

        # send out our enter messages as needed
        ch.room = new_room
        if ex.enter_mssg != '':
            message(ch, None, None, None, True, "to_room", ex.enter_mssg)
        elif dirnum == None:
            message(ch, None, None, None, True, "to_room", "$n has arrived.")
        else:
            message(ch, None, None, None, True, "to_room",
                    "$n arrives from the " + dir_name[dir_opp[dirnum]] + ".")

    # return our exit, whether it existed or not
    return ex

def try_move(ch, dir):
    '''Handles all moving of characters from one room to another, through
       commands. Attempts a move. If successful, returns the exit left
       through.'''
    ex = ch.room.exit(dir)

    # did we find an exit?
    if ex == None or not ch.cansee(ex):
        ch.send("Alas, there is no exit in that direction.")
    elif ex.is_closed:
        exname = ex.name
        if exname == '':
            exname = "it"
        ch.send("You will have to open " + exname + " first.")
    elif ex.dest == None:
        ch.send("It doesn't look like " + exname + " leads anywhere!")
    else:
        old_room = ch.room

        # run our leave hooks
        hooks.run("exit", hooks.build_info("ch rm ex", (ch, ch.room, ex)))

        ch.room = ex.dest
        ch.act("look")

        # run our enter hooks
        hooks.run("enter", hooks.build_info("ch rm", (ch, ch.room)))

    # return the exit we found (if we found any)
    return ex

def cmd_move(ch, cmd, arg):
    '''cmd_move is the basic entry to all of the movement utilities. See
       try_move() in movement.py'''
    try_move_mssg(ch, cmd)



################################################################################
# mud commands
################################################################################
add_cmd("north",     "n",  cmd_move, "standing", "flying", "player", True, True)
add_cmd("west",      "w",  cmd_move, "standing", "flying", "player", True, True)
add_cmd("east",      "e",  cmd_move, "standing", "flying", "player", True, True)
add_cmd("south",     "s",  cmd_move, "standing", "flying", "player", True, True)
add_cmd("up",        "u",  cmd_move, "standing", "flying", "player", True, True)
add_cmd("down",      "d",  cmd_move, "standing", "flying", "player", True, True)
add_cmd("northwest", None, cmd_move, "standing", "flying", "player", True, True)
add_cmd("northeast", None, cmd_move, "standing", "flying", "player", True, True)
add_cmd("southwest", None, cmd_move, "standing", "flying", "player", True, True)
add_cmd("southeast", None, cmd_move, "standing", "flying", "player", True, True)
add_cmd("nw",        None, cmd_move, "standing", "flying", "player", True, True)
add_cmd("ne",        None, cmd_move, "standing", "flying", "player", True, True)
add_cmd("sw",        None, cmd_move, "standing", "flying", "player", True, True)
add_cmd("se",        None, cmd_move, "standing", "flying", "player", True, True)

add_cmd("wake",      None, cmd_wake,"sleeping","sleeping", "player", True, True)
add_cmd("sleep",     None, cmd_sleep,"sitting", "flying",  "player", True, True)
add_cmd("stand",     None, cmd_stand,"sitting", "flying",  "player", True, True)
add_cmd("land",      None, cmd_stand, "flying", "flying",  "player", True, True)
add_cmd("sit",       None, cmd_sit, "standing", "flying",  "player", True, True)

# The mud needs to know our command for movement as well
set_cmd_move(cmd_move)
