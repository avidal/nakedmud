"""
universal_newline.py

This module makes sure that all newlines are in a universal form (\r\n)
regardless of how people send them out. \r, \n, \n\r, and \r\n all get
transformed to \r\n via this module
"""
import mud, mudsock, hooks



################################################################################
# universal newline hook
################################################################################
def universal_newline_hook(info):
    sock,  = hooks.parse_info(info)
    buf    = sock.outbound_text
    newbuf = []
    cr     = False

    i = 0
    while i < len(buf):
        if not (buf[i] == '\r' or buf[i] == '\n'):
            newbuf.append(buf[i])
        elif buf[i] == '\r':
            if len(newbuf) == 0 or not newbuf[-1] == '\n':
                cr = True
                newbuf.append(buf[i])
        else: # buf[i] == '\n':
            if cr == False:
                newbuf.append('\r')
            cr = False
            newbuf.append(buf[i])
        i = i + 1
    if cr == True:
        newbuf.append('\n')

    sock.outbound_text = ''.join(newbuf)



################################################################################
# initializing and unloading our hooks
################################################################################
hooks.add("process_outbound_text", universal_newline_hook)

def __unload__():
    '''detaches our colour module from the game'''
    hooks.remove("process_outbound_text", universal_newline_hook)
