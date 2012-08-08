/* list */
/* This module is an implementation of a linked list or vector.  It will basically be
 * used to store arbitrary-length lists.  This should be reasonably thread-safe, or at
 * lest thread-resistant. */

#ifndef _LIST_H_
#define _LIST_H_

#include <sys/types.h>
#include "types.h"

/* A member of the list.  This is prone to change, and should not be referenced */
typedef struct _list_member_t
{
	void *value;
	struct _list_member_t *next;
} list_member_t;

/* A definition of a list.  I shouldn't have to say that any elements of this shouldn't
 * be messed with. */
typedef struct
{
	list_member_t *first;
	BOOLEAN locked;
} list_t;

/* Create and return a new list */
list_t *list_create();
/* This should be called when a list is no longer being used.  It frees memory and stuff.
 * I don't recommend using this in a threaded environment.. */
void list_destroy(list_t *list);

/* Add a new entry to the beginning of the list */
void list_add_beginning(list_t *list, void *data);
/* Add a new entry to the end of the list. */
void list_add_end(list_t *list, void *data);

/* Remove and return the first entry in the list.  If there are no entries, NULL is returned */
void *list_remove_beginning(list_t *list);
/* Remove and return the last entry in the list.  If there are no entries, NULL is returned */
void *list_remove_end(list_t *list);
/* Remove a certain entry.  Note that this does pointer-comparison, so it has to be the same
 * actual memory that's supplied, not just equivalent */
void *list_remove_value(list_t *list, void *value);

/* Get the value at a certain index of the list.  If the element doesn't exist, NULL is returned */
void *list_get_element(list_t *list, uint32_t num);
/* Remove the value at a certain index of the list.  
 * NOTE: updating should be locked so that this doesn't change while removing it */
void *list_remove_element(list_t *list, uint32_t num);

/* Check if the specified value is in the list */
BOOLEAN list_contains(list_t *list, void *value);

/* Get the number of elements in the list */
uint32_t list_get_count(list_t *list);

/* Get the list as an array of void*'s.  The number returned is returned in the "num" parameter. 
 * NOTE: The returned array has to be free()'d! */
void** list_get_array(list_t *list, uint32_t *num);

/* Display the list; this is more for debugging than anything, it's not really useful for 
 * anything else */
void list_print(list_t *list);

#endif

