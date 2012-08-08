/* user */
/* This represents a connected or logged in user.  It contains their state, 
 * their name (if possible), the channel they're in (if possible), and
 * their client/server token
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "user.h"
#include "room.h"

const char *user_states[] = { "CONNECTED", "SENT_CLIENT_INFORMATION", "SENT_AUTHENTICATION", "JOINED_CHANNEL", "DEAD" };

/* Create a new, empty user.  They start in state CONNECTED, with a NULL username, 
 * a blank client token, and a random server token */
user_t *create_user(int socket, char *ip)
{
	user_t *new_user = malloc(sizeof(user_t));
	
	new_user->socket = socket;
	strcpy(new_user->username, "Not logged in");
	new_user->state = CONNECTED;
	new_user->client_token = 0;
	new_user->server_token = rand();
	strncpy(new_user->ip, ip, IP_LENGTH);
	new_user->ip[IP_LENGTH - 1] = '\0';
	new_user->room = NULL;

	return new_user;
}
/* Clean up the user */
void destroy_user(user_t *user)
{
	if(user->room)
		free(user->room);
	free(user);
}

/* Get the user's socket */
int get_socket(user_t *user)
{
	return user->socket;
}

/* Set the username for the user, this should happen after they've authenticated */
void set_username(user_t *user, char *username)
{
	strncpy(user->username, username, MAX_NAME - 1);
	user->username[MAX_NAME - 1] = '\0';
}
/* Retrieve the username for the user */
char *get_username(user_t *user)
{
	return user->username;
}
/* Retrieve the ip for the user */
char *get_ip(user_t *user)
{
	return user->ip;
}
/* Set the state for the user.  This module doesn't care what the state is, so make 
 * sure it's a valid transition */
void set_user_state(user_t *user, user_states_t new_state)
{
	user->state = new_state;
}
/* Get the state for this user */
user_states_t get_user_state(user_t *user)
{
	return user->state;
}
/* Set the client token.  This is received when the user sends his information */
void set_client_token(user_t *user, uint32_t token)
{
	user->client_token = token;
}
/* Get the client token that was previously set */
uint32_t get_client_token(user_t *user)
{
	return user->client_token;
}
/* Get the server token, this is a random value generated when a user instance is
 * created */
uint32_t get_server_token(user_t *user)
{
	return user->server_token;
}

/* The name of the current chatroom, to save me a lot of time.
 * WARNING: returns a pointer to a static string, don't muck around with it  */
char *get_user_room(user_t  *user)
{
	return user->room;
}

/* Set the name of hte current room */
void set_user_room(user_t *user, char *room_name)
{
	if(user->room)
		free(user->room);
	user->room = NULL;

	if(room_name)
	{	
		user->room = malloc(strlen(room_name) + 1);
		strncpy(user->room, room_name, strlen(room_name) + 1);
	}
}

/* Display the user; for debugging */
void print_user(user_t *user)
{
	printf("Username: %s\n", user->username);
	printf("State: %d\n", user->state);
	printf("Client/server tokens: %08x/%08x\n\n", user->client_token, user->server_token);
	
}

/* Turn the user state into a string.  This string may NOT be modified! */
const char *get_user_state_string(user_t *user)
{
	if(get_user_state(user) < CONNECTED || get_user_state(user) > JOINED_CHANNEL)
		return "state unknown";
	else
		return user_states[get_user_state(user)];
}


/*
int main(int argc, char *argv[])
{
	srand(time(NULL));
	user_t *user = create_user(0);

	print_user(user);
	set_client_token(user, 0xbaadf00d);
	set_state(user, SENT_CLIENT_INFORMATION);
	set_username(user, "This is a username");
	print_user(user);

	return 0;
}
*/

