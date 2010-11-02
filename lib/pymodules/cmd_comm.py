################################################################################
#
# cmd_comm.c
#
# Various commands used in NakedMud(tm) for communicating with other
# characters, and NPCs.
#
################################################################################
from mud import *
from mudsys import add_cmd
import inform, hooks



def cmd_ask(ch, cmd, arg):
    '''cmd_ask is used to pose a question to another character. Mostly, this is
       intended to be used to carry on dialogs with NPCs. Ask has a local range
       (i.e. you can only ask people in the same room as you questions)
       usage: ask <person> [about] <question>

       examples:
         ask bob about cats           ask bob about the topic, "cats"
         ask jim can I have a salad?  ask jim if you can have a salad'''
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
    '''cmd_tell sends a message to another character. Primarily intended for
       player-player communication. Players can tell other players things even
       if they are not in the same room.
       usage: tell <person> <mesage>
       
       examples:
         tell luke I am your father'''
    try:
        tgt, mssg = parse_args(ch, True, cmd, arg, "ch.world.noself string")
    except: return

    mssg = mssg.replace("$", "$$")
    message(ch, tgt, None, None, False, "to_vict",
            "{r$n tells you, '" + mssg + "'{n")
    message(ch, tgt, None, None, False, "to_char",
            "{rYou tell $N, '" + mssg + "'{n")

def cmd_chat(ch, cmd, arg):
    '''cmd_chat sends a message to all of the players currently logged on.
       usage: chat <message>

       example:
         chat hello, world!'''
    if arg == '':
        ch.send("Chat what?")
    else:
        arg = arg.replace("$", "$$")
        message(ch, None, None, None, False, "to_world",
                "{y$n chats, '" + arg + "'{n")
        message(ch, None, None, None, False, "to_char",
                "{yyou chat, '" + arg + "'{n")

def cmd_say(ch, cmd, arg):
    '''cmd_say sends a message to everyone in the same room as you. Say, like
       ask, can trigger NPC dialogs.
       usage: say <message>

       example:
         say hello, room!'''
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
    '''NPCs with dialogs will often have something to say when you
       greet/approach then. cmd_greet is a way to get them talking.
       usage: greet <person>

       examples:
         greet mayor'''
    try:
        tgt, = parse_args(ch, True, cmd, arg, "ch.room.noself")
    except: return

    message(ch, tgt, None, None, False, "to_char", "You greet $N.")
    message(ch, tgt, None, None, False, "to_vict", "$n greets you.")
    message(ch, tgt, None, None, False, "to_room", "$n greets $N.")

    # run greet hooks
    hooks.run("greet", hooks.build_info("ch ch", (ch, tgt)))

def cmd_emote(ch, cmd, arg):
    '''Send a special text message to the room you are in. The message is
       preceded by your name, unless you put a $n somewhere in the text, in
       which case the $n is replaced by your name.
       usage: emote <message>

       examples:
         emote does a little dance.
         emote A gunshot sounds, and $n is laying on the ground, dead.'''
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
    '''cmd_gemote is similar to emote, but it sends a global message'''
    if arg == '':
        ch.send("Gemote we must, but gemote what?")
    else:
        # same as emote, but global
        if arg.find("$n") == -1:
            arg = "$n " + arg
        message(ch, None, None, None, False, "to_world, to_char",
                "{bGLOBAL:{c " + arg + "{n")

def cmd_page(ch, cmd, arg):
    '''Send a message to another character, and also make it beep'''
    try:
        tgt, mssg = parse_args(ch, True, cmd, arg, "ch.world.noself string")
    except: return
    ch.send("\007\007You page " + inform.see_char_as(ch, tgt))
    tgt.send("\007\007*" + inform.see_char_as(tgt, ch) + "* " + mssg)



################################################################################
# add our commands
################################################################################
add_cmd("ask",     None, cmd_ask,   "sitting", "flying", "player", True, False)
add_cmd("say",     None, cmd_say,   "sitting", "flying", "player", True, False)
add_cmd("'",       None, cmd_say,   "sitting", "flying", "player", True, False)
add_cmd("tell",    None, cmd_tell,  "sitting", "flying", "player", True, False)
add_cmd("chat",    None, cmd_chat,  "sitting", "flying", "player", True, False)
add_cmd("gossip",  None, cmd_chat,  "sitting", "flying", "player", True, False)
add_cmd("\"",      None, cmd_chat,  "sitting", "flying", "player", True, False)
add_cmd("page",    None, cmd_page,  "sitting", "flying", "player", True, False)
add_cmd("greet",   None, cmd_greet, "sitting", "flying", "player", True, False)
add_cmd("approach",None, cmd_greet, "sitting", "flying", "player", True, False)
add_cmd("emote",   None, cmd_emote, "sitting", "flying", "player", True, False)
add_cmd("gemote",  None, cmd_gemote,"sitting", "flying", "player", True, False)
add_cmd(":",       None, cmd_emote, "sitting", "flying", "player", True, False)
