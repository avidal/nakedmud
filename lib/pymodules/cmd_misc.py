################################################################################
#
# cmd_misc.c
#
# a collection of miscellaneous commands that come with NakedMud(tm)
#
################################################################################
from mud import *
from hooks import *
from mudsys import add_cmd, add_cmd_check
import event, mudsys


def cmd_stop(ch, cmd, arg):
    '''stop performing the character\'s current action'''
    if not ch.isActing():
        ch.send("But you're not currently performing an action!\r\n")
    else:
        ch.interrupt()

def cmd_clear(ch, cmd, arg):
    '''clear the screen'''
    ch.send_raw("\033[H\033[J")

def event_delayed_cmd(ch, filler, cmd):
    '''used to perform delayed commands'''
    ch.act(cmd, True)

def cmd_delay(ch, cmd, arg):
    '''Perform a command, but delay its execution by a couple seconds'''
    try:
        secs, to_delay = parse_args(ch, True, cmd, arg, "double string")
    except: return

    if secs < 1:
        ch.send("You can only delay commands for positive amounts of time.")
    else:
        ch.send("You delay '%s' for %.2f seconds" % (to_delay, secs))
        event.start_event(ch, secs, event_delayed_cmd, None, to_delay)

def cmd_motd(ch, cmd, arg):
    '''Displays the MOTD to the character'''
    ch.page(get_motd())

def cmd_save(ch, cmd, arg):
    '''save the character'''
    mudsys.do_save(ch)
    ch.send("Saved.")

def cmd_quit(ch, cmd, arg):
    '''quit the game'''
    log_string(ch.name + " has left the game.")
    mudsys.do_save(ch)
    mudsys.do_quit(ch)



################################################################################
# add our commands
################################################################################
add_cmd("stop",  None, cmd_stop,  "player", False)
add_cmd("clear", None, cmd_clear, "player", False)
add_cmd("delay", None, cmd_delay, "player", False)
add_cmd("motd",  None, cmd_motd,  "player", False)
add_cmd("save",  None, cmd_save,  "player", False)
add_cmd("quit",  None, cmd_quit,  "player", True)

chk_can_save = lambda ch, cmd: ch.is_pc
add_cmd_check("save", chk_can_save)
add_cmd_check("quit", chk_can_save)
