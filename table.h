/* table */
/* This module is an implementation of a hashtable.  For now, I'm going to implement it
 * as a linked list with a linear search.  Definitely not the best solution, not even 
 * close, but I'll go back and fix it if there's time. */
/* NOTE: These functions are NOT thread-safe. */
/* TODO: implement with a hashtable */

#ifndef _TABLE_H_
#define _TABLE_H_

#include <sys/types.h>

/* A member of the table.  This is prone to change, and should not be referenced */
typedef struct _table_member_t
{
	char *key;
	void *value;
	struct _table_member_t *next;
} table_member_t;

/* A definition of a table.  I shouldn't have to say that any elements of this shouldn't
 * be messed with. */
typedef struct
{
	table_member_t *first;
} table_t;

/* Create and return a new table */
table_t *table_create();
/* This should be called when a table is no longer being used.  It frees memory and stuff. */
void table_destroy(table_t *table);

/* Add a new key:value pair to the table.  If they key is already in the table, then the value
 * is replaced */
void table_add(table_t *table, char *key, void *value);
/* Find and return the value for the specified key.  Returns NULL if the key wasn't found */
void *table_find(table_t *table, char *key);
/* Remove and return the value for the specified key.  Returns NULL if the key wasn't found */
void *table_remove(table_t *table, char *key);
/* Get an array of all keys in the table, in no paricular order.  The number of keys returned 
 * is returned in count. 
 * NOTE: This has to be free'd! Also, this returns pointers to the ACTUAL keys, which will 
 * be free'd when the table is destroyed, so if you intend to use them for long-term make
 * a copy. */
char **get_keys(table_t *table, size_t *count);
/* Get an array of all values in the table, in no paricular order.  The number of values 
 * returned is returned in count. 
 * NOTE: This has to be free'd! */
void **get_values(table_t *table, size_t *count);
/* Get the number of entries in the table */
size_t table_get_count(table_t *table);

/* Display the table; this is more for debugging than anything, it's not really useful for 
 * anything else */
void table_print(table_t *table);

#endif

