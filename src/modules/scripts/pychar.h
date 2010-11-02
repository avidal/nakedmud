#ifndef __PYCHAR_H
#define __PYCHAR_H
//*****************************************************************************
//
// py_char.h
//
// A python extention to allow python scripts to treat MUD characters as an
// object within the script.
//
//*****************************************************************************

/* initialize characters for use */
PyMODINIT_FUNC
init_PyChar(void);
int PyChar_AsUid(PyObject *ch);
int PyChar_Check(PyObject *value);

PyObject *
newPyChar(CHAR_DATA *ch);

#endif //__PYCHAR_H
