/* room */
/* This module represents a single chatroom.  It contains a list of sockets, and
 * can provide the sockets for the select() call on demand.  It is also possible
 * to add or remove sockets from the list.  When data arrives on one of the sockets, 
 * this module is notified and the data is processed, often mirrored to all the other
 * sockets. */

#ifndef _ROOM_H_
#define _ROOM_H_

#include <stdint.h>
#include <unistd.h>

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

#define MIN_ROOM_LENGTH 1
#define MAX_ROOM_LENGTH 16
#define MAX_TOPIC_LENGTH 1024

#include "packet_buffer.h"
#include "table.h"
#include "user.h"

typedef struct
{
	/* Each element in this list is a user_t */
	table_t *users;

	/* The name of the channel.  This is set when it's created, then never changed */
	char name[MAX_ROOM_LENGTH];
	/* The topic of the channel.  This can be changed at any time by anybody */
	char topic[MAX_TOPIC_LENGTH];

} room_t;

typedef enum
{
	ROOM_SUCCESS,
	ROOM_RESTRICTED,
	ROOM_NAME_TOO_SHORT,
	ROOM_NAME_TOO_LONG,
	ROOM_INVALID_NAME
	
} room_error_codes_t;


/* Create a new room instance with no users, the specified name, and a blank topic */
room_t *room_create(char *name);
/* Destroy the room instance */
void room_destroy(room_t *room);

/* Get the name */
char *room_get_name(room_t *room);
/* Get the topic */
char *room_get_topic(room_t *room);

/* Add a user to the room.  The given user should already be authenticated, and has 
 * requested to join this room.  This will automatically trigger a server message. */
void room_add_user(room_t *room, user_t *user);
/* Remove the specified user from the room.  This will automatically trigger a server
 * message */
void room_remove_user(room_t *room, user_t *user);
/* Send a message to everybody in the room */
void room_message(room_t *room, uint32_t message_subtype, char *from, char *message);
/* Send a packet to everybody in the room */
void room_packet(room_t *room, packet_buffer_t *packet);
/* Set a new topic to the room.  This will automatically broadcast a server message */
void room_set_topic(room_t *room, char *new_topic);
/* Get the number of users in the room */
size_t room_get_count(room_t *room);

/* Get the list of users who are currently in the channel.  It has to be freed. */
user_t **room_get_users(room_t *room, size_t *count);

/* This will send the list of users who are currently in the room to the specified socket
 * as a series of "EID_USER_IN_CHANNEL" packets */
void room_send_users_in_channel(room_t *room, int s);


#endif

