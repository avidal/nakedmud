'''
path.py

Plugs into the routine module to allow for the easy construction of paths and
path following.
'''
from mud import *
from mudsys import add_cmd
import mud, mudsys



################################################################################
# functions
################################################################################
def leads_to(frm, to):
    '''returns whether from leads directly to to'''
    for ex in frm.exnames:
        if frm.exit(ex).dest is to:
            return True
    return False

def shortest_path_bfs(frm, to, ignore_doors = False, ignore = None):
    '''calculates the shortest path, but uses a breadth first search. More
       efficient than depth-first seach for very short paths with lots of
       branches or very large muds.'''
    if frm == to:
        return [frm]

    rooms = []
    depth = []

    if ignore == None:
        ignore = set()
    ignore.add(frm)

    # what is our highest depth
    i = 1

    # the index of the room our last depth started at
    j = 0

    # append ourself and our depth
    rooms.append(frm)
    depth.append(i)

    # keep going until we find To, or we can't go any deeper
    found = False
    while not found:
        prev_depth = rooms[j:]
        for rm in prev_depth:
            for ex in rm.exnames:
                dest = rm.exit(ex).dest
                if dest in ignore:
                    continue
                rooms.append(dest)
                depth.append(i)
                if dest is to:
                    found = True
                    break
                ignore.add(rm)
            if found:
                break

        i += 1
        j += len(prev_depth)

    # go backwards from our destination
    rooms.reverse()
    depth.reverse()

    # first step, pull out our destination room
    path = [to]

    # pull out all other rooms on the shortest path
    while len(rooms) > 0:
        curr_depth = depth[0]
        
        # figure out which room in our current layer
        # links to our previous room in the shortest path
        for next in rooms:
            if leads_to(next, path[len(path)-1]):
                path.append(next)
                break

        # clear our last layer
        i = 0
        while i < len(depth) and depth[i] == curr_depth:
            i += 1
        rooms = rooms[i:]
        depth = depth[i:]

    # put it back in order
    path.reverse()
    return path

def shortest_path_dfs(frm, to, ignore_doors = False, ignore = None):
    '''returns the steps needed to take to go from one room to another. More
       efficient than breadth-first search for very long paths with only a few
       branches, or very small muds.'''
    path = []

    if ignore == None:
        ignore = set()

    # a list of rooms we ignore for tracking. Add ourself so we don't loop back
    ignore.add(frm)

    # if we're the target room, return an empty list
    if frm is to:
        return [frm]

    # build the shortest path 
    for ex in frm.exnames:
        # check if it has a door, and if we ignore it
        if frm.exit(ex).is_closed:
            continue

        # get the dest room. if there is none, skip this exit
        next_room = frm.exit(ex).dest
        
        if next_room is None:
            continue

        # if we already know this is a dead end or a loopback, skip it
        if next_room in ignore:
            continue

        next_path = shortest_path(next_room, to, ignore_doors, ignore)

        # dead end
        if len(next_path) == 0:
            continue

        # we found a path, append ourself
        next_path.insert(0, frm)

        # are we shorter than the previous one?
        if len(path) == 0 or len(path) > len(next_path):
            path = next_path

    return path

# set whether we are using bfs or dfs as our main pathing method
shortest_path = shortest_path_bfs

def path_to_dirs(path):
    '''takes a path of rooms and converts it to directions'''

    dirs = []
    i    = 0
    while i < len(path) - 1:
        for ex in path[i].exnames:
            if path[i].exit(ex).dest == path[i+1]:
                dirs.append(ex)
                break
        i = i + 1

    # return the directions we generated, if any
    return dirs

def build_patrol(rms, reverse = True):
    '''builds a set of directions that need to be followed to do a patrol
       between the rooms. If reverse is true, also supplies the directions
       to loop back on itself'''
    if reverse == True:
        loopback = [x for x in rms]
        loopback.reverse()
        rms      = rms + loopback[1:]

    path = []
    i    = 0
    while i < len(rms) - 1:
        path = path + shortest_path(rms[i], rms[i+1])
        i += 1
    return path_to_dirs(path)

def step(frm, to, ignore_doors = False):
    '''returns the first step needed to take to go from one room to another'''
    steps = shortest_path(frm, to, ignore_doors)
    if steps == None or len(steps) <= 1:
        return None
    return path_to_dirs(steps)[0]



################################################################################
# commands
################################################################################
def cmd_path(ch, cmd, arg):
    '''This is really just for purposes of testing to make sure the module
       works properly'''
    try:
        tgt, = parse_args(ch, True, cmd, arg, "ch.world.noself")
    except: return

    path = build_patrol([ch.room, tgt.room])
    if len(path) == 0:
        ch.send("Path doesn't exist")
    else:
        ch.send(str(path))



################################################################################
# initialization
################################################################################

# add our commands
add_cmd("path", None, cmd_path, "sitting", "flying", "admin", False, False)

# mud initialization
mud.build_patrol = build_patrol
