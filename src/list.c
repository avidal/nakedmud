//*****************************************************************************
//
// list.c
//
// the implementation of a generic list structure
//
//*****************************************************************************

#include <stdlib.h>
#include "list.h"


struct list_node {
  void *elem;
  struct list_node *next;
};

struct list_iterator {
  struct list      *L;
  struct list_node *curr;
};

struct list {
  struct list_node *head;
  struct list_node *tail;
  int size;
};

#ifndef FALSE
#define FALSE   0
#define TRUE    !(FALSE)
#endif


//
// Delete a list node, and all nodes attached to it
//
void deleteListNode(struct list_node *N) {
  if(N->next) deleteListNode(N->next);
  free(N);
};

//
// delete the list node, plus it's element using the supplied function
//
void deleteListNodeWith(struct list_node *N, void (*delete_func)(void *)) {
  if(N->next) deleteListNodeWith(N->next, delete_func);
  delete_func(N->elem);
  free(N);
}

//
// Create a new list node containing the given element
//
struct list_node *newListNode(void *elem) {
  struct list_node *N = malloc(sizeof(struct list_node));
  N->elem = elem;
  N->next = NULL;
  return N;
};




//*****************************************************************************
//
// List interface functions. Documentation in list.h
//
//*****************************************************************************
struct list *newList() {
  struct list *L = malloc(sizeof(struct list));
  L->head = NULL;
  L->tail = NULL;
  L->size = 0;
  return L;
};


void deleteList(struct list *L) {
  if(L->head) deleteListNode(L->head);
  free(L);
};

void deleteListWith(struct list *L, void *func) {
  if(L->head) deleteListNodeWith(L->head, func);
  free(L);
}

void listPut(struct list *L, void *elem) {
  if(listIn(L, elem))
    return;

  struct list_node *N = newListNode(elem);
  N->next = L->head;
  L->head = N;
  L->size++;
  if(L->size == 1)
    L->tail = N;
};


void listQueue(struct list *L, void *elem) {
  if(listIn(L, elem))
    return;

  struct list_node *N = newListNode(elem);

  if(L->head == NULL) {
    L->head = N;
    L->tail = N;
  }
  else {
    L->tail->next = N;
    L->tail       = N;
  }
  L->size++;
}


void *listPop(struct list *L) {
  return listRemoveNum(L, 0);
};


void listPush(struct list *L, void *elem) {
  listPut(L, elem);
};


int listIn(struct list *L, const void *elem) {
  struct list_node *N = L->head;

  while(N) {
    if(N->elem == elem)
      return TRUE;
    N = N->next;
  }

  return FALSE;
};


int listRemove(struct list *L, const void *elem) {
  struct list_node *N = L->head;

  // we don't have any contents
  if(N == NULL)
    return FALSE;

  // we first have to check if it is the head
  if(N->elem == elem) {
    L->head = N->next;
    N->next = NULL;
    deleteListNode(N);
    L->size--;
    if(L->size == 0)
      L->tail = NULL;
    return TRUE;
  }
  // otherwise, we can check if it's another element
  else {
    while(N->next) {
      // we found it ... remove it now
      if(N->next->elem == elem) {
	if(N->next == L->tail)
	  L->tail = N;
	struct list_node *tmp = N->next;
	N->next = tmp->next;
	tmp->next = NULL;
	deleteListNode(tmp);
	L->size--;
	return TRUE;
      }
      N = N->next;
    }
  }
  // we didn't find it
  return FALSE;
};

void *listRemoveNum(struct list *L, int num) {
  void *elem = listGet(L, num);
  if(elem) listRemove(L, elem);
  return elem;
}

int listSize(struct list *L) {
  return L->size;
}

void *listGet(struct list *L, int num) {
  struct list_node *node = L->head;
  int i;

  for(i = 0; i < num && node; i++)
    node = node->next;

  if(node) return node->elem;
  else     return NULL;
}

int isListEmpty(struct list *L) {
  return (L->head == NULL);
}

void *listGetWith(struct list *L, const void *cmpto, void *func) {
  int (* comparator)(const void *, const void *) = func;
  struct list_node *node = L->head;

  for(node = L->head; node != NULL; node = node->next)
    if(!comparator(cmpto, node->elem))
      return node->elem;
  return NULL;
}

int listRemoveWith(struct list *L, const void *cmpto, void *func) {
  int (* comparator)(const void *, const void *) = func;
  struct list_node *N = L->head;

  // we don't have any contents
  if(N == NULL)
    return FALSE;

  // we first have to check if it is the head
  if(!comparator(cmpto, N->elem)) {
    L->head = N->next;
    N->next = NULL;
    deleteListNode(N);
    L->size--;
    if(L->size == 0)
      L->tail = NULL;
    return TRUE;
  }
  // otherwise, we can check if it's another element
  else {
    while(N->next) {
      // we found it ... remove it now
      if(!comparator(cmpto, N->next->elem)) {
	if(N->next == L->tail)
	  L->tail = N;
	struct list_node *tmp = N->next;
	N->next = tmp->next;
	tmp->next = NULL;
	deleteListNode(tmp);
	L->size--;
	return TRUE;
      }
      N = N->next;
    }
  }
  // we didn't find it
  return FALSE;
}


void listPutWith(struct list *L, void *elem, void *func) {
  int (* comparator)(const void *, const void *) = func;
  struct list_node *N = L->head;

  // we don't have any contents, or we're lower than the
  // first list content then just put it at the start
  if(N == NULL || comparator(elem, N->elem) < 0)
    listPut(L, elem);
  else {
    // while we've got a next element, compare ourselves to it.
    // if we are smaller than it, then sneak inbetween. Otherwise,
    // skip to the next element
    while(N->next != NULL) {
      int val = comparator(elem, N->next->elem);
      // we're less than or equal to it... sneak in
      if(val <= 0) {
	struct list_node *new_node = newListNode(elem);
	new_node->next = N->next;
	N->next = new_node;
	L->size++;
	return;
      }
      else
	N = N->next;
    }
    // if we've gotten this far, then we need to attach ourself to the end
    N->next = newListNode(elem);
    L->tail = N->next;
    L->size++;
  }
}


void listSortWith(struct list *L, void *func) {
  // make a new list, and just pop our elements
  // into it while we still have 'em
  struct list *new_list = newList();

  while(listSize(L) > 0)
    listPutWith(new_list, listPop(L), func);

  L->head = new_list->head;
  L->tail = new_list->tail;
  L->size = new_list->size;
  // FREE ... not delete. Delete would kill the things
  // we just transferred over to the old list
  free(new_list);
}


struct list *listCopyWith(struct list *L, void *func) {
  void *(* copy_func)(void *) = func;
  struct list *newlist = newList();

  struct list_node *N = NULL;
  for(N = L->head; N; N = N->next)
    listQueue(newlist, copy_func(N->elem));

  return newlist;
}


//*****************************************************************************
//
// The functions for the list iterator interface. Documentation is in list.h
//
//*****************************************************************************
struct list_iterator *newListIterator(struct list *L) {
  struct list_iterator *I = malloc(sizeof(struct list_iterator));
  I->L    = L;
  I->curr = I->L->head;
  return I;
};

void deleteListIterator(struct list_iterator *I) {
  free(I);
};

void *listIteratorNext(struct list_iterator *I) {
  if(I->curr)
    I->curr = I->curr->next;

  if(I->curr)
    return I->curr->elem;
  else        
    return NULL;
};

void listIteratorReset(struct list_iterator *I) {
  I->curr = I->L->head;
};

void *listIteratorCurrent(struct list_iterator *I) {
  if(I->curr)
    return I->curr->elem;
  else
    return NULL;
};
