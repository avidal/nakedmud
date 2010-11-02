#ifndef __PYROOM_H
#define __PYROOM_H
//*****************************************************************************
//
// pyroom.h
//
// A python extention to allow python scripts to treat MUD rooms as an
// object within the script.
//
//*****************************************************************************

/* initialize characters for use */
PyMODINIT_FUNC
init_PyRoom(void);

int PyRoom_AsVnum(PyObject *room);
int PyRoom_Check(PyObject *value);

PyObject *
newPyRoom(ROOM_DATA *room);

#endif //__PYROOM_H
