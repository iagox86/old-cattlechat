/* list */
/* This module is an implementation of a linked list or vector.  It will basically be
 * used to store arbitrary-length lists.  This should be reasonably thread-safe, or at
 * lest thread-resistant. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "list.h"
#include "types.h"

/* Wait to obtain a lock on the list.  This provides some level of thread-protection, but isn't
 * immune to race conditions. */
/* TODO: use mutex */
static void list_lock(list_t *list)
{
	/* A busy-wait is, of course, extremely bad practice, and I should be shot for using it, but
	 * it's the easiest way to do this */
	while(list->locked)
		;
	list->locked = TRUE;
}
/* Unlock the list */
static void list_unlock(list_t *list)
{
	list->locked = FALSE;
} 

/* Create and return a new list */
list_t *list_create()
{
	list_t *new_list = malloc(sizeof(list_t));
	new_list->first = NULL;
	new_list->locked = FALSE;

	return new_list;
}
/* This should be called when a list is no longer being used.  It frees memory and stuff.
 * I don't recommend using this in a threaded environment.. */
void list_destroy(list_t *list)
{
	list_member_t *node;
	list_member_t *old = NULL;

	node = list->first;
	while(node)
	{
		old = node;
		node = node->next;
		free(old);
	}
	free(list);
}

/* Add a new entry to the beginning of the list */
void list_add_beginning(list_t *list, void *value)
{
	list_member_t *new_node;

	/* Obtain a lock on writing to the list */
	list_lock(list);

	new_node = malloc(sizeof(list_member_t));
	new_node->next = list->first;
	new_node->value = value;

	list->first = new_node;

	/* Release our lock on writing to the list */
	list_unlock(list);
}
/* Add a new entry to the end of the list. */
void list_add_end(list_t *list, void *value)
{
	list_member_t *node;
	list_member_t *old = NULL;
	list_member_t *new_node;

	/* Obtain a lock on writing to the list */
	list_lock(list);

	new_node = malloc(sizeof(node));
	new_node->value = value;
	new_node->next = NULL;

	node = list->first;
	while(node)
	{
		old = node;
		node = node->next;
	}

	if(old)
		old->next = new_node;
	else
		list->first = new_node;

	/* Release our lock on writing to the list */
	list_unlock(list);
}

/* Remove and return the first entry in the list.  If there are no entries, NULL is returned */
void *list_remove_beginning(list_t *list)
{
	list_member_t *first = NULL;
	void *first_value = NULL;

	/* Obtain a lock on writing to the list */
	list_lock(list);

	if(list->first != NULL)
	{
		first = list->first;
		list->first = first->next;

		first_value = first->value;
		free(first);
	}

	/* Release our lock on writing to the list */
	list_unlock(list);
	
	return first_value;
}
/* Remove and return the last entry in the list.  If there are no entries, NULL is returned */
void *list_remove_end(list_t *list)
{
	void *value = NULL;
	list_member_t *node;
	list_member_t *old = NULL;
	list_member_t *older = NULL;

	/* Obtain a lock on writing to the list */
	list_lock(list);

	node = list->first;
	while(node)
	{
		older = old;
		old = node;
		node = node->next;
	}

	if(old)
	{

		/* Update older's next pointer to NULL, if possible */
		if(older)
			older->next = NULL;
		else
		{
			assert(old = list->first);
			list->first = NULL;
		}

		/* Free old */
		value = old->value;
		free(old);
	}

	/* Release our lock on writing to the list */
	list_unlock(list);

	return value;
}

/* Remove a certain entry.  Note that this does pointer-comparison, so it has to be the same
 * actual memory that's supplied, not just equivalent */
void *list_remove_value(list_t *list, void *value)
{
	void *return_value = NULL;
	list_member_t *node;
	list_member_t *old = NULL;

	/* Obtain a lock on writing to the list */
	list_lock(list);

	node = list->first;
	while(node && return_value == NULL)
	{
		if(value == node->value)
		{
			if(old)
				old->next = node->next;
			else
				list->first = node->next;

			return_value = value;
		}

		old = node;
		node = node->next;
	}
	

	/* Release our lock on writing to the list */
	list_unlock(list);

	return return_value;
}

/* Get the value at a certain index of the list.  If the element doesn't exist, NULL is returned */
void *list_get_element(list_t *list, uint32_t num)
{
	uint32_t i;
	list_member_t *node;
	void *value = NULL;

	/* Obtain a lock on writing to the list */
	list_lock(list);

	node = list->first;

	for(i = 0; i < num && node; i++)
		node = node->next;

	if(node)
		value = node->value;

	/* Release our lock on writing to the list */
	list_unlock(list);

	return value;
}
/* Remove the value at a certain index of the list.  
 * NOTE: updating should be locked so that this doesn't change while removing it */
void *list_remove_element(list_t *list, uint32_t num)
{
	uint32_t i;
	list_member_t *node;
	list_member_t *old = NULL;
	void *value = NULL;

	/* Obtain a lock on writing to the list */
	list_lock(list);

	if(num == 0 && list->first)
	{
		old = list->first;
		value = list->first->value;
		list->first = list->first->next;
	}
	else
	{
		node = list->first;
		for(i = 0; i < num && node; i++)
		{
			old = node;
			node = node->next;
		}

		if(node)
		{
			old->next = node->next;
			value = node->value;
			free(node);
		}
	}

	/* Release our lock on writing to the list */
	list_unlock(list);

	return value;
}

/* Check if the specified value is in the list */
BOOLEAN list_contains(list_t *list, void *value)
{
	list_member_t *node;

	node = list->first;
	while(node)
	{
		if(node->value == value)
			return TRUE;
		node = node->next;
	}

	return FALSE;
}

/* Get the number of elements in the list */
/* List has to be locked when this is called */
uint32_t list_get_count(list_t *list)
{
	uint32_t i = 0;
	list_member_t *node;

	node = list->first;
	while(node)
	{
		i++;
		node = node->next;
	}

	return i;
}

/* Get the list as an array of void*'s.  The number returned is returned in the "num" parameter. 
 * NOTE: The returned array has to be free()'d! */
void** list_get_array(list_t *list, uint32_t *num)
{
	void **array;
	uint32_t i;
	list_member_t *node;

	/* Obtain a lock on writing to the list */
	list_lock(list);

	*num = list_get_count(list);
	array = malloc(*num * sizeof(void*));

	node = list->first;
	for(i = 0; i < *num && node; i++)
	{
		array[i] = node->value;
		node = node->next;
	}

	/* Release our lock on writing to the list */
	list_unlock(list);

	return array;
}

/* Display the list; this is more for debugging than anything, it's not really useful for 
 * anything else */
void list_print(list_t *list)
{
	list_member_t *node;
	uint32_t i = 0;

	/* Obtain a lock on writing to the list */
	list_lock(list);

	node = list->first;

	while(node)
	{
		i++;
		printf("%3d. %8d [%p]\n", i, (int) node->value, node->next);
		node = node->next;
	}
	printf(" ==> Elements: %d\n", list_get_count(list));

	/* Release our lock on writing to the list */
	list_unlock(list);
}


/**** TEST FUNCTIONS ****/


/*static list_t *get_list()
{
	list_t *list = list_create();

	list_add_beginning(list, (void*) 2);
	list_add_beginning(list, (void*) 1);
	list_add_end(list, (void*) 3);
	list_add_end(list, (void*) 4);

	return list;
}

 int main(int argc, char *argv[])
{
	list_t *list = get_list();
	uint32_t num;
	void **test_list;

	printf("Should be 1 2 3 4:\n");
	list_print(list);

	list_destroy(list);
	printf("\n\n=========================\n\n");
	list = get_list();

	printf("Should be 1 ==> %d\n", list_remove_beginning(list));
	printf("Should be 2 ==> %d\n", list_remove_beginning(list));
	printf("Should have 3 4:\n");
	list_print(list);
	printf("Should be 3 ==> %d\n", list_remove_beginning(list));
	printf("Should be 4 ==> %d\n", list_remove_beginning(list));
	printf("Should be 0 ==> %d\n", list_remove_beginning(list));
	printf("Should be empty:\n");
	list_print(list);

	list_destroy(list);
	printf("\n\n=========================\n\n");
	list = get_list();

	printf("Should be 4 ==> %d\n", list_remove_end(list));
	printf("Should be 3 ==> %d\n", list_remove_end(list));
	printf("Should have 1 2:\n");
	list_print(list);
	printf("Should be 2 ==> %d\n", list_remove_end(list));
	printf("Should be 1 ==> %d\n", list_remove_end(list));
	printf("Should be 0 ==> %d\n", list_remove_end(list));
	printf("Should be empty:\n");
	list_print(list);

	list_destroy(list);
	printf("\n\n=========================\n\n");
	list = get_list();

	printf("Should be 2 ==> %d\n", list_remove_value(list, 2));
	printf("Should have 1, 3, 4:\n");
	list_print(list);
	printf("Should be 0 ==> %d\n", list_remove_value(list, 2));
	printf("Should have 1, 3, 4:\n");
	list_print(list);
	printf("Should be 1 ==> %d\n", list_remove_value(list, 1));
	printf("Should have 3, 4:\n");
	list_print(list);
	printf("Should be 4 ==> %d\n", list_remove_value(list, 4));
	printf("Should have 3:\n");
	list_print(list);
	printf("Should be 3 ==> %d\n", list_remove_value(list, 3));
	printf("Should be empty:\n");
	list_print(list);

	list_destroy(list);
	printf("\n\n=========================\n\n");
	list = get_list();

	printf("Should be 0 ==> %d\n", list_get_element(list, -1));
	printf("Should be 1 ==> %d\n", list_get_element(list, 0));
	printf("Should be 2 ==> %d\n", list_get_element(list, 1));
	printf("Should be 3 ==> %d\n", list_get_element(list, 2));
	printf("Should be 4 ==> %d\n", list_get_element(list, 3));
	printf("Should be 0 ==> %d\n", list_get_element(list, 4));

	list_destroy(list);
	printf("\n\n=========================\n\n");
	list = get_list();

	printf("Should be 0 ==> %d\n", list_remove_element(list, -1));
	printf("Should have 1,2,3,4:\n");
	list_print(list);
	printf("Should be 1 ==> %d\n", list_remove_element(list, 0));
	printf("Should have 2,3,4:\n");
	list_print(list);
	printf("Should be 2 ==> %d\n", list_remove_element(list, 0));
	printf("Should have 3,4:\n");
	list_print(list);
	printf("Should be 0 ==> %d\n", list_remove_element(list, 2));
	printf("Should have 3,4:\n");
	list_print(list);
	printf("Should be 4 ==> %d\n", list_remove_element(list, 1));
	printf("Should have 3:\n");
	list_print(list);
	printf("Should be 3 ==> %d\n", list_remove_element(list, 0));
	printf("Should be empty:\n");
	list_print(list);


	list_destroy(list);
	printf("\n\n=========================\n\n");
	list = get_list();

	test_list = list_get_array(list, &num);
	printf("Should be 4 1 2 3 4: %d %d %d %d %d\n", num, test_list[0], test_list[1], test_list[2], test_list[3]);
	free(test_list);

	list_remove_element(list, 1);
	test_list = list_get_array(list, &num);
	printf("Should be 3 1 3 4: %d %d %d %d\n", num, test_list[0], test_list[1], test_list[2]);
	free(test_list);

	list_remove_element(list, 0);
	test_list = list_get_array(list, &num);
	printf("Should be 2 3 4: %d %d %d\n", num, test_list[0], test_list[1]);
	free(test_list);

	list_remove_element(list, 1);
	test_list = list_get_array(list, &num);
	printf("Should be 1 3: %d %d\n", num, test_list[0]);
	free(test_list);

	list_remove_element(list, 0);
	test_list = list_get_array(list, &num);
	printf("Should be 0: %d\n", num);
	free(test_list);

	return 0;
}*/
