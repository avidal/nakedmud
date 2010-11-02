#ifndef COMMAND_H
#define COMMAND_H
//*****************************************************************************
//
// command.h
//
// A structure that holds all of the data tied to player commands. 
// Specifically, the function that is executed and all of the restrictions on
// its execution. For commands that are written as python functions, newPyCmd
// is used, with the python function replacing the C function. The Python
// function should take the same 3 arguments.
//
//*****************************************************************************

typedef struct cmd_data CMD_DATA;

#define CMD_PTR(name) \
  void (* name)(CHAR_DATA *ch, const char *cmd, char *arg)
#define COMMAND(name) \
  void name(CHAR_DATA *ch, const char *cmd, char *arg)
#define CMD_CHK_PTR(name) \
  bool (* name)(CHAR_DATA *ch, const char *cmd) 
#define CMD_CHK(name) \
  bool name(CHAR_DATA *ch, const char *cmd)

CMD_DATA   *newCmd(const char *name, COMMAND(func), const char *user_group,
		   bool interrupts);
CMD_DATA *newPyCmd(const char *name, void  *pyfunc, const char *user_group,
		   bool interrupts);

void     deleteCmd(CMD_DATA *cmd);
CMD_DATA  *cmdCopy(CMD_DATA *cmd);
void     cmdCopyTo(CMD_DATA *from, CMD_DATA *to);

bool    charTryCmd(CHAR_DATA *ch, CMD_DATA *cmd, char *arg);

const char      *cmdGetName(CMD_DATA *cmd);
const char *cmdGetUserGroup(CMD_DATA *cmd);
bool       cmdGetInterrupts(CMD_DATA *cmd);
void            cmdAddCheck(CMD_DATA *cmd, CMD_CHK(func));
void          cmdAddPyCheck(CMD_DATA *cmd, void *pyfunc);

CMD_CHK(chk_can_move);     // ensures the person is standing
CMD_CHK(chk_conscious);    // ensures the person is conscious
CMD_CHK(chk_grounded);     // ensures the person is on the ground
CMD_CHK(chk_not_mob);      // ensures the person is not an NPC

#endif // COMMAND_H
