#ifndef __LIST_H
#define __LIST_H
//*****************************************************************************
//
// list.h
//
// the interface for a generic list structure. This list isn't a list in the
// typical way; it can only hold one of any one data at a time (i.e. character
// bob can only exist in the list once). I cannot think of any time in a MUD
// setting where someone would exist in a list more than once, so this list was
// designed with this in mind. If you want to use it otherwise, it should be
// pretty easy to comment out the checks :)
//
//*****************************************************************************

//
// Create a new list
//
struct list *newList();


//
// Delete an existing list
//
void deleteList();


//
// Delete the list. Also delete all of its contents with the function
// that is passed in. The function should take one argument, and should be
// compatible with the type of data in the list
//
void deleteListWith(struct list *L, void *func);


//
// Add an element to the list
//
void listPut(struct list *L, void *elem);


//
// Add an element to the end of the list
//
void listQueue(struct list *L, void *elem);


//
// Return true if the element is in the list. False otherwise
//
int listIn(struct list *L, const void *elem);


//
// Remove all instances of the elem from the list. Return
// true if successful, and false otherwise.
//
int listRemove(struct list *L, const void *elem);


//
// Remove the element in the list at the specified place,
// and return it
//
void *listRemoveNum(struct list *L, int num);


//
// remove the first element in the list, and return it
//
void *listPop(struct list *L);


//
// add the item to the head of the list
//
void listPush(struct list *L, void *elem);


//
// How many elements does the list have?
//
int listSize(struct list *L);

//
// Is the list empty?
//
int isListEmpty(struct list *L);


//
// get the element with the specific number
//
void *listGet(struct list *L, int num);


//
// Put the element in the list in an ascending order, based on
// what the comparator, func, tells us is the order.
//
void listPutWith(struct list *L, void *elem, void *func);


//
// get an element... use a function that is passed in to find the right element
// the function should return a boolean value (TRUE if we have a match) and
// only take one argument. cmpto must be the thing we are compared against
// in func.
//
void *listGetWith(struct list *L, const void *cmpto, void *func);


//
// Similar to listGetWith, but removes the element permenantly. Returns
// TRUE if the item was found and removed. FALSE other wise. Cmpto must
// be the thing we are compared against in func.
//
void *listRemoveWith(struct list *L, const void *cmpto, void *func);


//
// Sorts the element in a list with the specified comparator function.
// Func takes two arguments, and returns 0 if the two match. -1 is returned
// if the first is less than the second, and 1 otherwise.
//
void listSortWith(struct list *L, void *func);


//
// Make a copy of the list. func is a function that takes one argument (the
// data that is in the list) and returns a copy of that data.
//
struct list *listCopyWith(struct list *L, void *func);


//
// Create an iterator to go over the list
//
struct list_iterator *newListIterator(struct list *L);


//
// Delete the list iterator (but not the contents)
//
void deleteListIterator(struct list_iterator *I);


//
// Point the list iterator back at the head of the list
//
void listIteratorReset(struct list_iterator *I);


//
// Skip to the next element in the list. Return the next element
// if one exists, and NULL otherwise.
//
void *listIteratorNext(struct list_iterator *I);


//
// return a pointer to the current list element
//
void *listIteratorCurrent(struct list_iterator *I);


#endif // __LIST_H
