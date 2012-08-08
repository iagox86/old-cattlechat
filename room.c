/* room */
/* This module represents a single chatroom.  It contains a list of sockets, and
 * can provide the sockets for the select() call on demand.  It is also possible
 * to add or remove sockets from the list.  When data arrives on one of the sockets, 
 * this module is notified and the data is processed, often mirrored to all the other
 * sockets. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

#include "output.h"
#include "packet_buffer.h"
#include "table.h"
#include "user.h"

#include "room.h"

/* Create a new room instance with no users, the specified name, and a blank topic */
room_t *room_create(char *name)
{
	room_t *new_room = malloc(sizeof(room_t));

	new_room->users = table_create();

	strncpy(new_room->name, name, MAX_ROOM_LENGTH - 1);
	new_room->name[MAX_ROOM_LENGTH - 1] = '\0';

	strcpy(new_room->topic, "No topic");

	return new_room;
}

/* Destroy the room instance */
void room_destroy(room_t *room)
{
	table_destroy(room->users);
	free(room);
}

/* Get the name */
char *room_get_name(room_t *room)
{
	return room->name;
}

/* Get the topic */
char *room_get_topic(room_t *room)
{
	return room->topic;
}

/* Add a user to the room.  The given user should already be authenticated, and has 
 * requested to join this room.  A server message should be sent to notify everybody
 * in the room */
void room_add_user(room_t *room, user_t *user)
{
	table_add(room->users, get_username(user), user);
}

/* Remove the specified user from the room.  A server message should be sent to notify
 * the users in the room. */
void room_remove_user(room_t *room, user_t *user)
{
	table_remove(room->users, get_username(user));
}

/* Send a message to everybody in the room */
void room_message(room_t *room, chatevent_subtype_t message_subtype, char *from, char *message)
{
	size_t i;
	size_t num_users;
	user_t **users = (user_t **) get_values(room->users, &num_users);
	packet_buffer_t *packet;

	/* (uint32_t) subtype -- the subtype of the event
	 * (ntstring) username  -- The username of the person who caused the event, if 
	 *  applicable
	 * (ntstring) text -- The text of the event, if applicable */
	packet = create_buffer(SID_CHATEVENT);
	add_int32(packet, message_subtype);
	add_ntstring(packet, from);
	add_ntstring(packet, message);

	for(i = 0; i < num_users; i++)
		send_buffer(packet, get_socket(users[i]));


	destroy_buffer(packet);
	free(users);
}

/* Send a packet to everybody in the room */
void room_packet(room_t *room, packet_buffer_t *packet)
{
	size_t i;
	size_t num_users;
	user_t **users = (user_t **) get_values(room->users, &num_users);

	for(i = 0; i < num_users; i++)
		send_buffer(packet, get_socket(users[i]));

	free(users);
}

/* Set a new topic to the room.  A server message should be broadcast when this occurs. */
void room_set_topic(room_t *room, char *new_topic)
{
	strncpy(room->topic, new_topic, MAX_TOPIC_LENGTH - 1);
	new_topic[MAX_TOPIC_LENGTH - 1] = '\0';
}

/* Get the number of users in the room */
size_t room_get_count(room_t *room)
{
	return table_get_count(room->users);
}


/* Get the list of users who are currently in the channel.  It has to be freed. */
user_t **room_get_users(room_t *room, size_t *count)
{
	return (user_t **) get_values(room->users, count);
}

/* This will send the list of users who are currently in the room to the specified socket
 * as a series of "EID_USER_IN_CHANNEL" packets */
void room_send_users_in_channel(room_t *room, int s)
{
	size_t i;
	size_t num_users;
	user_t **users = (user_t **) get_values(room->users, &num_users);
	packet_buffer_t *packet;

	for(i = 0; i < num_users; i++)
	{
		/* (uint32_t) subtype -- the subtype of the event
		 * (ntstring) username  -- The username of the person who caused the event, if 
		 *  applicable
		 * (ntstring) text -- The text of the event, if applicable */
		packet = create_buffer(SID_CHATEVENT);

		add_int32(packet, EID_USER_IN_CHANNEL);
		add_ntstring(packet, get_username(users[i]));
		add_ntstring(packet, "");

		send_buffer(packet, s);
		destroy_buffer(packet);
	}

	free(users);
}

