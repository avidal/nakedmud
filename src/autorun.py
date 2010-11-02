#!/usr/bin/python
################################################################################
#
# autorun.py
#
# This autorun script was designed to be used with NakedMud. It will 
# automagically restart your MUD any time it crashes. If the MUD is manually
# shut down, the autorun script will terminate. 
#
# Geoff Hollis
# http://www.ualberta.ca/~hollis/nakedmud.html
#
################################################################################

from time import sleep   # we delay before restarting
from os   import system  # to start running the MUD

# how long do we delay before a restart (seconds)
restart_delay = 5

# the path to the MUD binary
path = './NakedMud'

# the port we will be running the MUD under
port = 4000

# the command we execute to boot up the MUD
cmd  = "%s %d" % (path, port)

# now, while we have not exited without an error, run the MUD 
# and reboot it every time we exit with an error (we crash)
while True:
    # run the MUD
    status = system(cmd)

    # exited normally
    if status == 0:
	break;
    else:
	# We should probably see if we can figure out what kind of
	# error caused the crash here, and report it

	# wait out our delay, then restart the MUD
	sleep(restart_delay)
