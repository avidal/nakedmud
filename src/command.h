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

CMD_DATA   *newCmd(const char *name, COMMAND(func), int min_pos, int max_pos,
		   const char *user_group, bool mob_ok, bool interrupts);
CMD_DATA *newPyCmd(const char *name, void  *pyfunc, int min_pos, int max_pos,
		   const char *user_group, bool mob_ok, bool interrupts);

void     deleteCmd(CMD_DATA *cmd);
CMD_DATA  *cmdCopy(CMD_DATA *cmd);
void     cmdCopyTo(CMD_DATA *from, CMD_DATA *to);

bool    charTryCmd(CHAR_DATA *ch, CMD_DATA *cmd, char *arg);

const char      *cmdGetName(CMD_DATA *cmd);
const char *cmdGetUserGroup(CMD_DATA *cmd);
int            cmdGetMinPos(CMD_DATA *cmd);
int            cmdGetMaxPos(CMD_DATA *cmd);
bool            cmdGetMobOk(CMD_DATA *cmd);
bool       cmdGetInterrupts(CMD_DATA *cmd);

#endif // COMMAND_H
