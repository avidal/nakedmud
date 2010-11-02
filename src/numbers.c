//*****************************************************************************
//
// numbers.c
//
// This provides a wrapper around integers, doubles, and longs so we can pass
// them around as pointers.
//
//*****************************************************************************

#include "mud.h"
#include "numbers.h"



//*****************************************************************************
// integers
//*****************************************************************************
struct int_struct {
  int val;
};

INTEGER *newInteger(int val) {
  INTEGER *integer = malloc(sizeof(INTEGER));
  integer->val = val;
  return integer;
}

void deleteInteger(INTEGER *integer) {
  free(integer);
}

int integerGetVal(INTEGER *integer) {
  return integer->val;
}

int integerCmp(INTEGER *int1, INTEGER *int2) {
  if(int1->val == int2->val)
    return 0;
  else if(int1->val > int2->val)
    return 1;
  else
    return -1;
}



//*****************************************************************************
// doubles
//*****************************************************************************
struct double_struct {
  double val;
};

DOUBLE *newDouble(double val) {
  DOUBLE *dbl = malloc(sizeof(DOUBLE));
  dbl->val = val;
  return dbl;
}

void deleteDouble(DOUBLE *dbl) {
  free(dbl);
}

double doubleGetVal(DOUBLE *dbl) {
  return dbl->val;
}

int doubleCmp(DOUBLE *dbl1, DOUBLE *dbl2) {
  if(dbl1->val == dbl2->val)
    return 0;
  else if(dbl1->val > dbl2->val)
    return 1;
  else
    return -1;
}



//*****************************************************************************
// longs
//*****************************************************************************
struct long_struct {
  long val;
};

LONG *newLong(long val) {
  LONG *lng = malloc(sizeof(LONG));
  lng->val  = val;
  return lng;
}

void deleteLong(LONG *lng) {
  free(lng);
}

long longGetVal(LONG *lng) {
  return lng->val;
}

int longCmp(LONG *lng1, LONG *lng2) {
  if(lng1->val == lng2->val)
    return 0;
  else if(lng1->val > lng2->val)
    return 1;
  else
    return -1;
}
