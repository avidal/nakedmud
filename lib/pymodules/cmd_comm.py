################################################################################
#
# cmd_comm.c
#
# Various commands used in NakedMud(tm) for communicating with other
# characters, and NPCs.
#
################################################################################
from mud import *
from mudsys import add_cmd, add_cmd_check
import inform, hooks



def cmd_ask(ch, cmd, arg):
    '''Usage: ask <person> [about] <question>

       This command is used to pose a question to another character. Mostly,
       this is intended to be used to carry on dialogs with NPCs. Ask has a
       local range (i.e. you can only ask questions to people in the same room
       as you.
       '''
    try:
        tgt, question = parse_args(ch, True, cmd, arg,
                                   "ch.room.noself [about] string")
    except: return

    question = question.replace("$", "$$")
    message(ch, tgt, None, None, False, "to_vict",
            "{w$n asks you, '" + question + "'{n")
    message(ch, tgt, None, None, False, "to_char",
            "{wYou ask $N, '" + question + "'{n")

    # run our ask hooks
    hooks.run("ask", hooks.build_info("ch ch str", (ch, tgt, question)))

def cmd_tell(ch, cmd, arg):
    '''Usage: tell <person> <message>

       This command sends a message to another character. Primarily intended
       for player-to-player communication. Players can tell other players
       things even if they are not in the same room.'''
    try:
        tgt, mssg = parse_args(ch, True, cmd, arg, "ch.world.noself string")
    except: return

    mssg = mssg.replace("$", "$$")
    message(ch, tgt, None, None, False, "to_vict",
            "{r$n tells you, '" + mssg + "'{n")
    message(ch, tgt, None, None, False, "to_char",
            "{rYou tell $N, '" + mssg + "'{n")

def cmd_chat(ch, cmd, arg):
    '''Usage: chat <message>

       This command will send a message to all players currently logged on.
       '''
    if arg == '':
        ch.send("Chat what?")
    else:
        arg = arg.replace("$", "$$")
        message(ch, None, None, None, False, "to_world",
                "{y$n chats, '" + arg + "'{n")
        message(ch, None, None, None, False, "to_char",
                "{yyou chat, '" + arg + "'{n")

def cmd_say(ch, cmd, arg):
    '''Usage: say <message>

      This command will send a message to everyone in the same room as you. Say,
      like ask, can trigger NPC dialogs.'''
    if arg == '':
        ch.send("Say what?")
    else:
        arg = arg.replace("$", "$$")
        message(ch, None, None, None, False, "to_room",
                "{y$n says, '" + arg + "'{n")
        message(ch, None, None, None, False, "to_char",
                "{yyou say, '" + arg + "'{n")        

        # run say hooks
        hooks.run("say", hooks.build_info("ch str", (ch, arg)))

def cmd_greet(ch, cmd, arg):
    '''Usage: greet <person>

       NPCs with dialogs will often have something to say when you greet or
       approach then. Greeting an NPC is a way to get them talking.'''
    try:
        tgt, = parse_args(ch, True, cmd, arg, "ch.room.noself")
    except: return

    message(ch, tgt, None, None, False, "to_char", "You greet $N.")
    message(ch, tgt, None, None, False, "to_vict", "$n greets you.")
    message(ch, tgt, None, None, False, "to_room", "$n greets $N.")

    # run greet hooks
    hooks.run("greet", hooks.build_info("ch ch", (ch, tgt)))

def cmd_emote(ch, cmd, arg):
    '''Usage: emote <text>

       Send a special text message to the room you are in. The message is
       preceded by your name, unless you put a $n somewhere in the text, in
       which case the $n is replaced by your name. For example:

       > emote A gunshot sounds, and $n is laying on the ground, dead.

       Would show a message to everyone in the room saying that you are dead
       to a gunshot.'''
    if arg == '':
        ch.send("Emote we must, but emote what?")
    else:
        # see if a $n is within the argument ... if there is, let the person
        # put his or her name where it's wanted. Otherwise, tag it onto the
        # front of the message
        if arg.find("$n") == -1:
            arg = "$n " + arg
        message(ch, None, None, None, False, "to_room, to_char", arg)

def cmd_gemote(ch, cmd, arg):
    '''Gemote is similar to emote, except that it sends a mud-wide message
       instead of a room-specific message.'''
    if arg == '':
        ch.send("Gemote we must, but gemote what?")
    else:
        # same as emote, but global
        if arg.find("$n") == -1:
            arg = "$n " + arg
        message(ch, None, None, None, False, "to_world, to_char",
                "{bGLOBAL:{c " + arg + "{n")

def cmd_page(ch, cmd, arg):
    '''Usage: page <person> <message>

       Paging a person will send them a message, as well as making a beeping
       sound on their computer to get their attention. Page can be used on
       anyone in the mud, regardless if you are in the same room as them or not.
       '''
    try:
        tgt, mssg = parse_args(ch, True, cmd, arg, "ch.world.noself string")
    except: return
    ch.send("\007\007You page " + inform.see_char_as(ch, tgt))
    tgt.send("\007\007*" + inform.see_char_as(tgt, ch) + "* " + mssg)



################################################################################
# add our commands
################################################################################
add_cmd("ask",     None, cmd_ask,   "player", False)
add_cmd("say",     None, cmd_say,   "player", False)
add_cmd("'",       None, cmd_say,   "player", False)
add_cmd("tell",    None, cmd_tell,  "player", False)
add_cmd("chat",    None, cmd_chat,  "player", False)
add_cmd("gossip",  None, cmd_chat,  "player", False)
add_cmd("\"",      None, cmd_chat,  "player", False)
add_cmd("page",    None, cmd_page,  "player", False)
add_cmd("greet",   None, cmd_greet, "player", False)
add_cmd("approach",None, cmd_greet, "player", False)
add_cmd("emote",   None, cmd_emote, "player", False)
add_cmd("gemote",  None, cmd_gemote,"player", False)
add_cmd(":",       None, cmd_emote, "player", False)

def chk_room_communication(ch, cmd):
    if ch.pos in ["sleeping", "unconscious"]:
        ch.send("You cannot do that while " + ch.pos + ".")
        return False

for cmd in ["ask", "say", "'", "greet", "approach", "emote", ":"]:
    add_cmd_check(cmd, chk_room_communication)
