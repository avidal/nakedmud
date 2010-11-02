################################################################################
#
# cmd_manip.py
#
# a set of commands that NakedMud(tm) comes with that allows characters to
# manipulate various things. These commands are mostly directed towards
# manipulating objects (e.g. get, put, drop, etc...) but can also affect other
# things like exits (e.g. open, close)
#
################################################################################
from mud import *
from utils import *
from inform import *
from mudsys import add_cmd, add_cmd_check
import movement, hooks



def do_give(ch, recv, obj):
    '''does the handling of the give command'''
    message(ch, recv, obj, None, True, "to_room",
            "$n gives $o to $N.")
    message(ch, recv, obj, None, True, "to_vict",
            "$n gives $o to you.")
    message(ch, recv, obj, None, True, "to_char",
            "You give $o to $N.")
    obj.carrier = recv

    # run our give hook
    hooks.run("give", hooks.build_info("ch ch obj", (ch, recv, obj)))

def cmd_give(ch, cmd, arg):
    '''Usage: give <object> [to] <person>

       Attempts to transfer an object from your inventory to the specified
       person. You can give multiple objects at a time by using the all.
       prefix. For example:

       > give all.cookie george

       Would give all of the cookies in your inventory to George. If you only
       want to give him the raisin cookie, which also happens to be the third
       cookie in your inventory, you can use a numeric prefix instead of
       the all prefix. For example:

       > give 3.cookie george'''
    try:
        to_give, multi, recv = parse_args(ch, True, cmd, arg,
                                          "[the] obj.inv.multiple " +
                                          "[to] ch.room.noself")
    except: return

    if multi == False:
        do_give(ch, recv, to_give)
    else:
        for obj in to_give:
            do_give(ch, recv, obj)

def do_get(ch, obj, cont):
    '''transfers an item from the ground to the character'''
    if is_keyword(obj.bits, "notake"):
        ch.send("You cannot take " + see_obj_as(ch, obj) + ".")
    elif cont != None:
        message(ch, None, obj, cont, True, "to_char", "You get $o from $O.")
        message(ch, None, obj, cont, True, "to_room", "$n gets $o from $O.")
        obj.carrier = ch
    else:
        message(ch, None, obj, None, True, "to_char", "You get $o.")
        message(ch, None, obj, None, True, "to_room", "$n gets $o.")
        obj.carrier = ch

        # run get hooks
        hooks.run("get", hooks.build_info("ch obj", (ch, obj)))

def try_get_from(ch, cont, arg):
    '''tries to get one item from inside another'''
    if not cont.istype("container"):
        message(ch, None, cont, None, True, "to_char", "$o is not a container.")
    elif cont.container_is_closed:
        message(ch, None, cont, None, True, "to_char", "$o is closed.")
    else:
        # find our count and name
        num, name = get_count(arg)

        # multi or single?
        if num == "all":
            list = find_all_objs(ch, cont.objs, name)
            for obj in list:
                do_get(ch, obj, cont)
        else:
            obj = find_obj(ch, cont.objs, num, name)
            if obj != None:
                do_get(ch, obj, cont)

def cmd_get(ch, cmd, arg):
    '''Usage: get [the] <item> [[from] <other item>]

       Attempts to move an object from the room to your inventory. If an
       addition argument is supplied, the command assumes it is a container and
       instead tries to move an object from the container to your inventory.'''
    try:
        arg, cont = parse_args(ch, True, cmd, arg,
                               "[the] word | [from] obj.room.inv.eq")
    except: return
    
    # are we doing get, or get-from?
    if cont != None:
        try_get_from(ch, cont, arg)
    else:
        # try to find the object in the room
        try:
            found, multi = parse_args(ch, True, cmd, arg, "obj.room.multiple")
        except: return

        # pick up all the items we want
        if multi == False:
            do_get(ch, found, None)
        else:
            for obj in found:
                do_get(ch, obj, None)

def do_drop(ch, obj):
    '''handles object dropping'''
    message(ch, None, obj, None, True, "to_char", "You drop $o.")
    message(ch, None, obj, None, True, "to_room", "$n drops $o.")
    obj.room = ch.room

    # run our drop hook
    hooks.run("drop", hooks.build_info("ch obj", (ch, obj)))

def cmd_drop(ch, cmd, arg):
    '''Usage: drop <item>

       Attempts to move an object from your inventory to the ground.'''
    try:
        found, multi = parse_args(ch, True, cmd, arg, "[the] obj.inv.multiple")
    except: return

    # are we dropping a list of things, or just one?
    if multi == False:
        do_drop(ch, found)
    else:
        for obj in found:
            do_drop(ch, obj)

def do_remove(ch, obj):
    '''handles equipment removing'''
    # try to put it to our inventory
    obj.carrier = ch

    # make sure it succeeded
    if obj.carrier != ch:
        ch.send("You were unable to remove " + see_obj_as(ch, obj) + ".")
    else:
        message(ch, None, obj, None, True, "to_char", "You remove $o.")
        message(ch, None, obj, None, True, "to_room", "$n removes $o.")

        # run our hooks
        hooks.run("remove", hooks.build_info("ch obj", (ch, obj)))

def cmd_remove(ch, cmd, arg):
    '''Usage: remove <item | all>

       Attempts to remove an item you have equipped. If you would like to
       remove everything you are wearing, you may instead specify \'all\'
       instead of a specific item. If you would like to remove all of a certain
       type of object (for instance, rings) you can use an all. prefix. For
       example:

       > remove all.ring

       This command will remove everything you are wearing with the \'ring\'
       keyword. If you would instead like to remove, say, the second thing you
       are wearing with the \'ring\' keyword, you can supply a numeric prefix.
       For example:

       > remove 2.ring'''
    try:
        found, multi = parse_args(ch, True, cmd, arg, "[the] obj.eq.multiple")
    except: return

    # are we removing one thing, or multiple things?
    if multi == False:
        do_remove(ch, found)
    else:
        for obj in found:
            do_remove(ch, obj)

def do_wear(ch, obj, where):
    '''handles object wearing'''
    if not obj.istype("worn"):
        ch.send("But " + see_obj_as(ch, obj) + " is not wearable.")
    elif ch.equip(obj, where):
        message(ch, None, obj, None, True, "to_char", "You wear $o.")
        message(ch, None, obj, None, True, "to_room", "$n wears $o.")
        # equip hooks are done in the C code
    else:
        message(ch, None, obj, None, True, "to_char", "You could not equip $o.")

def cmd_wear(ch, cmd, arg):
    '''Usage: wear <item> [where]

       Attempts to equip an item from your inventory. If you would like to
       equip it to a non-default location, you can supply where on your body
       you would like to wear it. For example, if you would like to equip a
       torch, but in your offhand instead of your mainhand:

       > wear torch offhand

       If an item covers multiple locations on your body, you can specify where
       all you would like to equip the item as a comma-separated list:

       > wear gloves left hand, right hand'''
    try:
        found, multi, where = parse_args(ch, True, cmd, arg,
                                         "[the] obj.inv.multiple | [on] string")
    except: return

    # are we wearing one thing, or multiple things?
    if multi == False:
        do_wear(ch, found, where)
    else:
        for obj in found:
            do_wear(ch, obj, where)

def do_put(ch, obj, cont):
    '''handles the putting of objects'''
    if obj == cont:
        ch.send("You cannot put " + see_obj_as(ch, obj) + " into itself.")
    # make sure we have enough room 
    elif obj.weight > cont.container_capacity - cont.weight + cont.weight_raw:
        ch.send("There is not enough room in " + see_obj_as(ch, cont) +
                " for " + see_obj_as(ch, obj) + ".")
    # do the move
    else:
        obj.container = cont
        message(ch, None, obj, cont, True, "to_char", "You put $o in $O.")
        message(ch, None, obj, cont, True, "to_room", "$n puts $o in $O.")

def cmd_put(ch, cmd, arg):
    '''Usage: put [the] <item> [in the] <container>

       Attempts to move an object from your inventory into a specified
       container. The container must be in the room, in your inventory, or
       worn.'''
    try:
        found, multi, cont = parse_args(ch, True, cmd, arg,
                                        "[the] obj.inv.multiple " +
                                        "[in the] obj.room.inv")
    except: return

    # make sure we have a container
    if not cont.istype("container"):
        ch.send(see_obj_as(ch, cont) + " is not a container.")
    elif cont.container_is_closed:
        ch.send(see_obj_as(ch, cont) + " is currently closed.")
    # do we have one or multiple items?
    elif multi == False:
        do_put(ch, found, cont)
    else:
        for obj in found:
            do_put(ch, obj, cont)

def try_manip_other_exit(room, ex, closed, locked):
    '''used by open, close, lock, and unlock. When an exit is manipulated on one
       side, it is the case that we'll want to do an identical manipulation on
       the other side. That's what we do here. Note: Can only do close OR lock
       with one call to this function. Cannot handle both at the same time!'''
    opp_dir = None
    if ex.dest == None:
        return

    # see if we can figure out the opposite direction
    if ex.opposite != '':
        opp_dir = ex.opposite
    else:
        # figure out the direction of the exit, and its opposite
        dirnum = movement.dir_index(room.exdir(ex))
        if dirnum != -1:
            opp_dir = movement.dir_name[movement.dir_opp[dirnum]]

    # do we have an opposite direction to manipulate?
    if opp_dir == None:
        return

    # do we have an opposite exit to manipulate?
    opp_ex = ex.dest.exit(opp_dir)
    if opp_ex != None:
        # figure out our name
        name = "an exit"
        if opp_ex.name != '':
            name = opp_ex.name

        # try to manipulate the exit
        if closed and not opp_ex.is_closed:
            opp_ex.close()
            ex.dest.send(name + " closes from the other side.")
        elif locked and opp_ex.is_closed and not opp_ex.is_locked:
            opp_ex.lock()
            ex.dest.send(name + " locks from the other side.")
        elif not closed and opp_ex.is_closed and not opp_ex.is_locked:
            opp_ex.open()
            ex.dest.send(name + " opens from the other side.")
        elif not locked and opp_ex.is_locked:
            opp_ex.unlock()
            ex.dest.send(name + " unlocks from the other side.")

def cmd_lock(ch, cmd, arg):
    '''Usage: lock <direction | door | container>

       Attempts to lock a specified door, direction, or container.'''
    try:
        found, type = parse_args(ch, True, cmd, arg,
                                 "[the] {obj.room.inv.eq exit }")
    except: return

    # what did we find?
    if type == "exit":
        ex = found
        if not ex.is_closed:
            ch.send(ex.name + " must be closed first.")
        elif ex.is_locked:
            ch.send(ex.name + " is already locked.")
        elif ex.key == '':
            ch.send("You cannot figure out how " + ex.name +" would be locked.")
        elif not has_proto(ch, ex.key):
            ch.send("You cannot seem to find the key.")
        else:
            message(ch, None, None, None, True, "to_char",
                    "You lock " + ex.name + ".")
            message(ch, None, None, None, True, "to_room",
                    "$n locks " + ex.name + ".")
            ex.lock()
            try_manip_other_exit(ch.room, ex, ex.is_closed, True)

    # type must be object
    else:
        obj = found
        if not obj.istype("container"):
            ch.send(see_obj_as(ch, obj) + " is not a container.")
        elif not obj.container_is_closed:
            ch.send(see_obj_as(ch, obj) + " is not closed.")
        elif obj.container_is_locked:
            ch.send(see_obj_as(ch, obj) + " is already locked.")
        elif obj.container_key == '':
            ch.send("You cannot figure out how to lock " + see_obj_as(ch, obj))
        elif not has_proto(ch, obj.container_key):
            ch.send("You cannot seem to find the key.")
        else:
            message(ch, None, obj, None, True, "to_char", "You lock $o.")
            message(ch, None, obj, None, True, "to_room", "$n locks $o.")
            obj.container_is_locked = True

def cmd_unlock(ch, cmd, arg):
    '''Usage: unlock <door | direction | container>

       Attempts to unlock the specified door, direction, or container.'''
    try:
        found, type = parse_args(ch, True,cmd,arg, "[the] {obj.room.inv exit }")
    except: return

    # what did we find?
    if type == "exit":
        ex = found
        if not ex.is_closed:
            ch.send(ex.name + " is already open.")
        elif not ex.is_locked:
            ch.send(ex.name + " is already unlocked.")
        elif ex.key == '':
            ch.send("You cannot figure out how " + ex.name +
                    " would be unlocked.")
        elif not has_proto(ch, ex.key):
            ch.send("You cannot seem to find the key.")
        else:
            message(ch, None, None, None, True, "to_char",
                    "You unlock " + ex.name + ".")
            message(ch, None, None, None, True, "to_room",
                    "$n unlocks " + ex.name + ".")
            ex.unlock()
            try_manip_other_exit(ch.room, ex, ex.is_closed, False)

    # must be an object
    else:
        obj = found
        if not obj.istype("container"):
            ch.send(see_obj_as(ch, obj) + " is not a container.")
        elif not obj.container_is_closed:
            ch.send(see_obj_as(ch, obj) + " is already open.")
        elif not obj.container_is_locked:
            ch.send(see_obj_as(ch, obj) + " is already unlocked.")
        elif obj.container_key == '':
            ch.send("You cannot figure out how to unlock "+see_obj_as(ch, obj))
        elif not has_proto(ch, obj.container_key):
            ch.send("You cannot seem to find the key.")
        else:
            message(ch, None, obj, None, True, "to_char", "You unlock $o.")
            message(ch, None, obj, None, True, "to_room", "$n unlocks $o.")
            obj.container_is_locked = False

def cmd_open(ch, cmd, arg):
    '''Usage: open [the] <direction | door | container>

       Attempts to open the speficied door, direction, or container.'''
    try:
        found, type = parse_args(ch, True,cmd,arg, "[the] {obj.room.inv exit }")
    except: return

    # is it an exit?
    if type == "exit":
        ex = found
        if not ex.is_closed:
            ch.send(ex.name + " is already open.")
        elif ex.is_locked:
            ch.send(ex.name + " must be unlocked first.")
        elif not ex.is_closable:
            ch.send(ex.name + " cannot be opened.")
        else:
            message(ch, None, None, None, True, "to_char",
                    "You open " + ex.name + ".")
            message(ch, None, None, None, True, "to_room",
                    "$n opens " + ex.name + ".")
            ex.open()
            try_manip_other_exit(ch.room, ex, False, ex.is_locked)
            hooks.run("open_door", hooks.build_info("ch ex", (ch, ex)))

    # must be an object
    else:
        obj = found
        if not obj.istype("container"):
            ch.send(see_obj_as(ch, obj) + " is not a container.")
        elif not obj.container_is_closed:
            ch.send(see_obj_as(ch, obj) + " is already open.")
        elif obj.container_is_locked:
            ch.send(see_obj_as(ch, obj) + " must be unlocked first.")
        elif not obj.container_is_closable:
            ch.send(see_obj_as(ch, obj) + " cannot be opened.")
        else:
            message(ch, None, obj, None, True, "to_char", "You open $o.")
            message(ch, None, obj, None, True, "to_room", "$n opens $o.")
            obj.container_is_closed = False
            hooks.run("open_obj", hooks.build_info("ch obj", (ch, obj)))

def cmd_close(ch, cmd, arg):
    '''Usage: close <direction | door | container>

       Attempts to close the specified door, direction, or container.'''
    try:
        found, type = parse_args(ch, True,cmd,arg, "[the] {obj.room.inv exit }")
    except: return

    # is it an exit?
    if type == "exit":
        ex = found
        if ex.is_closed:
            ch.send(ex.name + " is already closed.")
        elif ex.is_locked:
            ch.send(ex.name + " must be unlocked first.")
        elif not ex.is_closable:
            ch.send(ex.name + " cannot be closed.")
        else:
            message(ch, None, None, None, True, "to_char",
                    "You close " + ex.name + ".")
            message(ch, None, None, None, True, "to_room",
                    "$n closes " + ex.name + ".")
            ex.close()
            try_manip_other_exit(ch.room, ex, True, ex.is_locked) 

    # must be an object
    else:
        obj = found
        if not obj.istype("container"):
            ch.send(see_obj_as(ch, obj) + " is not a container.")
        elif obj.container_is_closed:
            ch.send(see_obj_as(ch, obj) + " is already closed.")
        elif not obj.container_is_closable:
            ch.send(see_obj_as(ch, obj) + " cannot be closed.")
        else:
            message(ch, None, obj, None, True, "to_char", "You close $o.")
            message(ch, None, obj, None, True, "to_room", "$n closes $o.")
            obj.container_is_closed = True



################################################################################
# load all of our commands
################################################################################
add_cmd("give",   None, cmd_give,   "player", True)
add_cmd("get",    None, cmd_get,    "player", True)
add_cmd("drop",   None, cmd_drop,   "player", True)
add_cmd("remove", None, cmd_remove, "player", True)
add_cmd("wear",   None, cmd_wear,   "player", True)
add_cmd("put",    None, cmd_put,    "player", True)
add_cmd("open",   None, cmd_open,   "player", True)
add_cmd("close",  None, cmd_close,  "player", True)
add_cmd("lock",   None, cmd_lock,   "player", True)
add_cmd("unlock", None, cmd_unlock, "player", True)

def chk_can_manip(ch, cmd):
    if not ch.pos in ["sitting", "standing", "flying"]:
        ch.send("You cannot do that while " + ch.pos + ".")
        return False

for cmd in ["give", "get", "drop", "remove", "wear", "put", "open", "close",
            "lock", "unlock"]:
    add_cmd_check(cmd, chk_can_manip)
    
