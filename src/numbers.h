#ifndef NUMBERS_H
#define NUMBERS_H
//*****************************************************************************
//
// numbers.h
//
// This provides a wrapper around integers, doubles, and longs so we can pass
// them around as pointers.
//
//*****************************************************************************

typedef struct int_struct      INTEGER;
typedef struct double_struct    DOUBLE;
typedef struct long_struct        LONG;

INTEGER *newInteger(int val);
void  deleteInteger(INTEGER *integer);
int   integerGetVal(INTEGER *integer);
int      integerCmp(INTEGER *int1, INTEGER *int2);

DOUBLE   *newDouble(double val);
void   deleteDouble(DOUBLE *dbl);
double doubleGetVal(DOUBLE *dbl);
int       doubleCmp(DOUBLE *dbl1, DOUBLE *dbl2);

LONG       *newLong(long val);
void     deleteLong(LONG *lng);
long     longGetVal(LONG *lng);
int         longCmp(LONG *lng1, LONG *lng2);

#endif // NUMBERS_H
