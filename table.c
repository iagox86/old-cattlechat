/* table */
/* This module is an implementation of a hashtable.  For now, I'm going to implement it
 * as a linked list with a linear search.  Definitely not the best solution, not even 
 * close, but I'll go back and fix it if there's time. */
/* TODO: implement with a hashtable */

#include <stdio.h>
#include <assert.h>
#include <malloc.h>
#include <string.h>

#include <sys/types.h>

#include "table.h"

/* Create and return a new table */
table_t *table_create()
{
	table_t *new_table = malloc(sizeof(table_t));

	new_table->first = NULL;

	return new_table;
}

/* This should be called when a table is no longer being used.  It frees memory and stuff. */
void table_destroy(table_t *table)
{
	table_member_t *node;
	table_member_t *old;

	node = table->first;
	while(node)
	{
		old = node;
		node = node->next;
		free(old->key);
		free(old);
	}
	free(table);
}

/* Add a new key:value pair to the table.  If they key is already in the table, then the value
 * is replaced */
void table_add(table_t *table, char *key, void *value)
{
	table_member_t *node;
	table_member_t *old = NULL;

	table_member_t *new = malloc(sizeof(table_member_t));
	/* Warning: if strncpy() isn't used, a heap overflow could occur here */
	new->key = malloc((strlen(key) + 1) * sizeof(char));
	strncpy(new->key, key, strlen(key) + 1);
	new->key[strlen(key)] = '\0';
	new->value = value;
	new->next = NULL;

	node = table->first;
	while(node)
	{
		if(!(strcmp(key, node->key)))
		{
			node->value = value;
			return;
		}
		old = node;
		node = node->next;
	}

	if(old)
		old->next = new;
	else
		table->first = new;

}

/* Find and return the value for the specified key.  Returns NULL if the key wasn't found */
void *table_find(table_t *table, char *key)
{
	table_member_t *node;

	node = table->first;
	while(node)
	{
		if(!strcmp(node->key, key))
			return node->value;

		node = node->next;
	}

	return NULL;
}

/* Remove and return the value for the specified key.  Returns NULL if the key wasn't found */
void *table_remove(table_t *table, char *key)
{
	table_member_t *node;
	table_member_t *old = NULL;
	void *ret;

	node = table->first;
	while(node)
	{
		if(!(strcmp(key, node->key)))
		{
			/* Found it! */
			if(old)
				old->next = node->next;
			else
				table->first = node->next;

			ret = node->value;
			free(node->key);
			free(node);

			return ret;
		}

		old = node;
		node = node->next;
	}

	return NULL;
}

/* Get an array of all keys in the table, in no particular order.  The number of keys 
 * returned is returned in count. 
 * NOTE: This has to be free'd! Also, this returns pointers to the ACTUAL keys, which will 
 * be free'd when the table is destroyed, so if you intend to use them for long-term make
 * a copy. */
char **get_keys(table_t *table, size_t *count)
{
	table_member_t *node;
	char **ret;
	int i;

	*count = 0;

	node = table->first;
	while(node)
	{
		(*count)++;
		node = node->next;
	}

	ret = malloc(*count * sizeof(char*));

	node = table->first;
	for(i = 0; i < *count; i++)
	{
		ret[i] = node->key;
		node = node->next;
	}

	return ret;	
}

/* Get an array of all values in the table, in no particular order.  The number of values 
 * returned is returned in count. 
 * NOTE: This has to be free'd! */
void **get_values(table_t *table, size_t *count)
{
	table_member_t *node;
	void **ret;
	int i;

	*count = 0;

	node = table->first;
	while(node)
	{
		(*count)++;
		node = node->next;
	}

	ret = malloc(*count * sizeof(void*));

	node = table->first;
	for(i = 0; i < *count; i++)
	{
		ret[i] = node->value;
		node = node->next;
	}

	return ret;	
}

/* Get the number of entries in the table */
/* TODO: this implementation sucks.  Find a better way. */
size_t table_get_count(table_t *table)
{
	table_member_t *node;
	size_t count = 0;

	node = table->first;
	while(node)
	{
		count++;
		node = node->next;
	}

	return count;
}

/* Display the table; this is more for debugging than anything, it's not really useful for 
 * anything else */
void table_print(table_t *table)
{
	table_member_t *node;
	int i = 0;

	node = table->first;

	while(node)
	{
		i++;
		printf("%3d - %s ==> %p [%p]\n", i, node->key, node->value, node->next);

		node = node->next;
	}
}

/*int main(int argc, char *argv[])
{
	table_t *table = table_create();
	char **test;
	void **test2;
	size_t count;
	size_t count2;
	int i;

	table_add(table, "test", (void*) 'A');
	table_add(table, "test 1", (void*) 1);
	table_add(table, "test 2", (void*) 2);
	table_add(table, "test 3", (void*) 3);
	table_add(table, "test 4", (void*) 4);
	table_add(table, "test 5", (void*) 5);
	table_add(table, "test 6", (void*) 6);
	table_add(table, "test", (void*) 'B');


	test = get_keys(table, &count);
	test2 = get_values(table, &count2);
	assert(count == count2);
	for(i = 0; i < count; i++)
		printf("%d. %s -> %d\n", i, test[i], (int)  test2[i]);
	printf("Returned %d keys\n\n", count);
	free(test);

	table_print(table);

	table_remove(table, "test");
	table_remove(table, "test 3");
	table_remove(table, "test 6");

	printf("Should display test 1, 2, 4, and 5; nothing else:\n");

	test = get_keys(table, &count);
	test2 = get_values(table, &count2);
	assert(count == count2);
	for(i = 0; i < count; i++)
		printf("%d. %s -> %d\n", i, test[i], (int)  test2[i]);
	printf("Returned %d keys\n\n", count);
	free(test);
	table_print(table);

	printf("Same table, but with 'New!' on the end:\n");
	table_add(table, "New!", (void*) 'C');

	test = get_keys(table, &count);
	test2 = get_values(table, &count2);
	assert(count == count2);
	for(i = 0; i < count; i++)
		printf("%d. %s -> %d\n", i, test[i], (int)  test2[i]);
	printf("Returned %d keys\n\n", count);
	free(test);
	table_print(table);

	printf("Should be '5' ==> %d\n", (int)table_find(table, "test 5"));
	printf("Should be '1' ==> %d\n", (int)table_find(table, "test 1"));
	printf("Should be 'C' ==> %c\n", (int)table_find(table, "New!"));

	table_destroy(table);

	return 0;
} */



