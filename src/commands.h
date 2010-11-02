#ifndef __COMMAND_H
#define __COMMAND_H
//*****************************************************************************
//
// commands.h
//
// contains a list of all commands available on the MUD.
//
//*****************************************************************************

/* admin.c */
COMMAND(cmd_goto);
COMMAND(cmd_copyover);
COMMAND(cmd_shutdown);
COMMAND(cmd_linkdead);
COMMAND(cmd_wizhelp);


/* builder.c */
COMMAND(cmd_dig);
COMMAND(cmd_fill);
COMMAND(cmd_load);
COMMAND(cmd_purge);
COMMAND(cmd_zreset);
COMMAND(cmd_sclist);
COMMAND(cmd_rlist);
COMMAND(cmd_mlist);
COMMAND(cmd_olist);
COMMAND(cmd_dlist);
COMMAND(cmd_zlist);


/* cmd_manip.c */
COMMAND(cmd_unlock);
COMMAND(cmd_lock);
COMMAND(cmd_put);
COMMAND(cmd_get);
COMMAND(cmd_drop);
COMMAND(cmd_wear);
COMMAND(cmd_remove);
COMMAND(cmd_give);
COMMAND(cmd_close);
COMMAND(cmd_open);


/* cmd_comm.c */
COMMAND(cmd_ask);
COMMAND(cmd_tell);
COMMAND(cmd_greet);
COMMAND(cmd_say);
COMMAND(cmd_chat);
COMMAND(cmd_emote);
COMMAND(cmd_gemote);


/* cmd_misc.c */
COMMAND(cmd_delay);
COMMAND(cmd_clear);
COMMAND(cmd_quit);
COMMAND(cmd_save);
COMMAND(cmd_commands);
COMMAND(cmd_compress);
COMMAND(cmd_tog_prf);
COMMAND(cmd_more);
COMMAND(cmd_back);


/* inform.c */
COMMAND(cmd_who);
COMMAND(cmd_look);
COMMAND(cmd_inventory);
COMMAND(cmd_equipment);
COMMAND(cmd_help);


/* olc.c */
COMMAND(cmd_redit);
COMMAND(cmd_medit);
COMMAND(cmd_oedit);
COMMAND(cmd_zedit);
COMMAND(cmd_wedit);
COMMAND(cmd_scedit);
COMMAND(cmd_dedit);


/* movement.c */
COMMAND(cmd_move);
COMMAND(cmd_sit);
COMMAND(cmd_stand);
COMMAND(cmd_wake);
COMMAND(cmd_sleep);
COMMAND(cmd_enter);


#endif // __COMMAND_H
