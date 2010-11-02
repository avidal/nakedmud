'''
account_handler.py

The login and creation of accounts, and handles all account procedures, loading
and deleting, of characters.
'''
import mud, mudsys, char, hooks, account, socket, event, telnetlib

# control sequences for squelching passwords
squelch   = telnetlib.IAC + telnetlib.WILL + telnetlib.ECHO
unsquelch = telnetlib.IAC + telnetlib.WONT + telnetlib.ECHO



def check_acct_name(name):
    '''Makes sure an account name is valid'''
    return (len(name) > 3 and len(name) < 13 and
            name[0].isalpha() and name.isalnum())

def acct_name_prompt(sock):
    sock.send_raw("What is your account name? ")

def acct_new_password_prompt(sock):
    sock.send_raw("What is your new password? " + squelch)

def acct_password_prompt(sock):
    sock.send_raw("What is your password? " + squelch)

def acct_confirm_password_prompt(sock):
    sock.send_raw("\r\nVerify your password? " + squelch)

def acct_wait_dns_prompt(sock):
    sock.send_raw("Resolving your internet address, have patience... ")

def acct_finish_prompt(sock):
    sock.send_raw("{c\r\n*** Press enter to finish account creation:{n ") 

def acct_finish_handler(sock, arg):
    # pop our input handler for finishing account generation
    sock.pop_ih()

    # log that the account created
    mud.log_string("New account '" + sock.account.name + "' has created.")

    # register and save the account to disk
    mudsys.do_register(sock.account)

def acct_wait_dns_handler(sock, arg):
    # do nothing
    return

def acct_name_handler(sock, arg):
    '''the first prompt a socket encounters; enter a name of an account or
       a new name of an account to create one'''
    if arg and mudsys.account_exists(arg):
        # logging on to an already existing account
        mud.log_string("Account '" + arg + "' is trying to connect.")
        acct = mudsys.load_account(arg)

        # attach our account to our socket. Put in mudsys to prevent scripts
        # from messing around with account and socket connections
        mudsys.attach_account_socket(acct, sock)
        sock.pop_ih()
        sock.push_ih(acct_menu_handler, acct_main_menu)
        sock.push_ih(acct_password_handler, acct_password_prompt)

    elif not check_acct_name(arg):
        sock.send(mud.format_string("That is an invalid account name. Your " \
                                    "account name must only consist of " \
                                    "characters and numbers, and it must be " \
                                    "4 and 12 characters in length. The first "\
                                    "character MUST be a letter. Please pick "\
                                    "another.", False))

    elif mudsys.account_creating(arg):
        sock.send("An account with that name is already creating.")

    else:
        # creating a new account
        mud.log_string("Account '" + arg + "' is trying to create.")

        # create our new account
        acct = mudsys.create_account(arg)
        if acct == None:
            sock.send("Could not create an account with that name.")
        else:
            mudsys.attach_account_socket(acct, sock)
            sock.pop_ih()
            sock.push_ih(acct_menu_handler, acct_main_menu)
            sock.push_ih(acct_finish_handler, acct_finish_prompt)
            sock.push_ih(acct_confirm_password_handler, acct_confirm_password_prompt)
            sock.push_ih(acct_new_password_handler, acct_new_password_prompt)


def acct_new_password_handler(sock, arg):
    '''asks a new account for a password'''
    if len(arg) > 0:
        # put in mudsys to prevent scripts from messing with passwords
        mudsys.set_password(sock.account, arg)
        sock.pop_ih()

def acct_confirm_password_handler(sock, arg):
    '''checks to see if our argument matches our password; if not, we re-enter
       our old password'''
    # password functions put in mudsys to prevent scripts from
    # messing with passwords
    if not mudsys.password_matches(sock.account, arg):
        sock.send("Passwords do not match.")
        sock.pop_ih()
        sock.push_ih(acct_confirm_password_handler,acct_confirm_password_prompt)
        sock.push_ih(acct_password_handler, acct_password_prompt)
    else:
        # Password matches. Keep it and go down a level
        sock.send_raw(unsquelch)
        sock.pop_ih()

def acct_password_handler(sock, arg):
    '''asks an account to verify its password'''
    # password functions put in mudsys to prevent scripts from
    # messing with passwords
    sock.send_raw(unsquelch)
    if not mudsys.password_matches(sock.account, arg):
        sock.send("Incorrect password.")
        sock.close()
    else:
        # password matches, pop our handler and go down a level
        sock.pop_ih()

def find_reconnect(name):
    '''searches through the character list for a PC whose name matches the
       supplied name'''
    name = name.lower()
    for ch in char.char_list():
        if ch.is_pc and name == ch.name.lower():
            return ch
    return None

def acct_load_char(sock, arg):
    '''loads a character attached to the account. Argument supplied must be a
       numeric value corresponding to the character'''
    arg = int(arg)
    if arg >= len(sock.account.characters()) or arg < 0:
        sock.send("Invalid choice!")
    else:
        # get the name
        name = sock.account.characters()[arg]

        # first, try a reconnect
        ch = find_reconnect(name)

        # no reconnect? Try loading our character
        if not ch == None:
            # swap the sockets
            old_sock = ch.sock

            # attach and detach put in mudsys to prevent scripts from
            # messing around with the connections between chars and sockets
            mudsys.detach_char_socket(ch)
            mudsys.attach_char_socket(ch, sock)
            
            if old_sock != None:
                old_sock.close()
            mud.log_string(ch.name + " has reconnected.")
            ch.send("You take over a body already in use.")
            sock.push_ih(mudsys.handle_cmd_input, mudsys.show_prompt)

        else:
            # load our character. Put in mudsys to prevent scripts from using it
            ch = mudsys.load_char(name)
            if ch == None:
                sock.send("The player file for " + name + " is missing.")
            elif (mudsys.sys_getval("lockdown") != '' and
                  not ch.isInGroup(mudsys.sys_getval("lockdown"))):
                sock.send("You are currently locked out of the mud.")
                mud.extract(ch)
            else:
                # attach put in mudsys to prevent scripts from messing with
                # character and socket links
                mudsys.attach_char_socket(ch, sock)
                if mudsys.try_enter_game(ch):
                    mud.log_string(ch.name + " has entered the game.")
                    sock.push_ih(mudsys.handle_cmd_input, mudsys.show_prompt)
                    ch.page(mud.get_motd())
                    ch.act("look")
                    hooks.run("enter", hooks.build_info("ch rm", (ch, ch.room)))
                else:
                    sock.send("There was a problem entering the game. " + \
                              "try again later.")
                    # detach put in mudsys t prevent scripts from messing with
                    # character and socket links
                    mudsys.detach_char_socket(ch, sock)
                    mud.extract(ch)

def acct_menu_handler(sock, arg):
    '''parses account commands (new character, enter game, quit, etc)'''
    if len(arg) == 0:
        return
    arg = arg[0].upper()
    if arg.isdigit():
        acct_load_char(sock, arg)
    elif arg == 'Q':
        sock.send("Come back soon!")
        mudsys.do_save(sock.account)
        sock.close()
    elif arg == 'P':
        sock.push_ih(acct_confirm_password_handler,acct_confirm_password_prompt)
        sock.push_ih(acct_new_password_handler, acct_new_password_prompt)
        sock.push_ih(acct_password_handler, acct_password_prompt)
    elif arg == 'N':
        if "player" in [x.strip() for x in mudsys.sys_getval("lockdown").split(",")]:
            sock.send("New characters are not allowed to be created at this time.")
        else:
            hooks.run("create_character", hooks.build_info("sk", (sock,)))
    else:
        sock.send("Invalid choice!")

def display_acct_chars(sock):
    '''shows the socket a prettied list of characters tied to the account it
       has attached. Prints three names per line.'''
    num_cols   = 3
    print_room = (80 - 10*num_cols)/num_cols
    fmt        = "  {c%2d{g) %-" + str(print_room) + "s"

    sock.send("{w\r\nPlay a Character:")
    i = 0
    for ch in sock.account.characters():
        sock.send_raw(fmt % (i, ch))
        if i % num_cols == num_cols - 1:
            sock.send_raw("\r\n")
        i = i + 1

    if not i % num_cols == 0:
        sock.send_raw("\r\n")

def acct_main_menu(sock):
    '''displays the main menu for the account and asks for a command'''
    if len(sock.account.characters()) > 0:
        display_acct_chars(sock)

    sock.send("\r\n{wAdditional Options:")
    sock.send("  {g[{cP{g]assword change")
    sock.send("  {g[{cN{g]new character\r\n")
    sock.send_raw("Enter choice, or Q to quit:{n ")



################################################################################
# events for blocking action when dns lookup is in progress
################################################################################
def dns_check_event(no_owner, unused, info):
    sock, = hooks.parse_info(info)
    if sock.can_use:
        sock.send("Lookup complete.")
        sock.pop_ih()
        sock.bust_prompt()
    else:
        event.start_event(None, 0.1, dns_check_event, None, info)



################################################################################
# account handler hooks
################################################################################
def account_handler_hook(info):
    # put a nonfunctional prompt up while waiting for the DNS to resolve
    sock, = hooks.parse_info(info)
    sock.push_ih(acct_name_handler, acct_name_prompt)
    sock.push_ih(acct_wait_dns_handler, acct_wait_dns_prompt)
    event.start_event(None, 0.1, dns_check_event, None, info)

def copyover_complete_hook(info):
    sock, = hooks.parse_info(info)
    sock.push_ih(acct_menu_handler, acct_main_menu)
    sock.push_ih(mudsys.handle_cmd_input, mudsys.show_prompt)



################################################################################
# loading and unloading the module
################################################################################
hooks.add("receive_connection", account_handler_hook)
hooks.add("copyover_complete",  copyover_complete_hook)

def __unload__():
    '''removes the hooks for account handling'''
    hooks.remove("receive_connection", account_handler_hook)
    hooks.remove("copyover_complete",  copyover_complete_hook)
