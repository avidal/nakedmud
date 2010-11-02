#ifndef SET_H
#define SET_H
//*****************************************************************************
//
// set.h
//
// a non-ordered container that has constant lookup time.
//
//*****************************************************************************

struct set_data  *newSet(int buckets);
void           deleteSet(struct set_data *set);
void              setPut(struct set_data *set, void *elem);
void           setRemove(struct set_data *set, void *elem);
int                setIn(struct set_data *set, void *elem);



//*****************************************************************************
//
// set iterator
//
// we may sometimes want to iterate across all of the elements in a set.
// this lets us do so.
//
//*****************************************************************************

struct set_iterator *newSetIterator(struct set_data *S);
void              deleteSetIterator(struct set_iterator *I);
void               setIteratorReset(struct set_iterator *I);
void               *setIteratorNext(struct set_iterator *I);
void            *setIteratorCurrent(struct set_iterator *I);

#endif // SET_H
