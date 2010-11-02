#ifndef __PYOBJ_H
#define __PYOBJ_H
//*****************************************************************************
//
// pyobj.h
//
// A python extention to allow python scripts to treat MUD objects as an
// object within the script.
//
//*****************************************************************************

/* initialize characters for use */
PyMODINIT_FUNC
init_PyObj(void);
int PyObj_AsUid(PyObject *ch);
int PyObj_Check(PyObject *value);

PyObject *
newPyObj(OBJ_DATA *obj);

#endif //__PYOBJ_H
