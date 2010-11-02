#ifndef SET_H
#define SET_H
//*****************************************************************************
//
// set.h
//
// a non-ordered container that has constant lookup time.
//
//*****************************************************************************

typedef struct set_data                   SET;
typedef struct set_iterator               SET_ITERATOR;

SET  *newSet   (int buckets);
void  deleteSet(SET *set);
void  setPut   (SET *set, void *elem);
void  setRemove(SET *set, void *elem);
int   setIn    (SET *set, void *elem);


//*****************************************************************************
//
// set iterator
//
// we may sometimes want to iterate across all of the elements in a set.
// this lets us do so.
//
//*****************************************************************************

SET_ITERATOR *newSetIterator    (SET *S);
void          deleteSetIterator (SET_ITERATOR *I);
void          setIteratorReset  (SET_ITERATOR *I);
void         *setIteratorNext   (SET_ITERATOR *I);
void         *setIteratorCurrent(SET_ITERATOR *I);

#endif // SET_H
