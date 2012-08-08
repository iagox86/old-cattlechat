/* user */
/* This represents a connected or logged in user.  It contains their state, 
 * their name (if possible), the channel they're in (if possible), and
 * their client/server token
 */


#ifndef _USER_H_
#define _USER_H_

#include <stdint.h>

#include "account.h"

#define IP_LENGTH 20

typedef enum
{
	/* They have just connected to the server.  The only thing they can do here
	 * is send their client information */
	CONNECTED,

	/* They've sent their client information, and the server information has been
	 * returned to them.  At this point, they may either log in, or create an account. 
	 * If log in or creation fails, this state is returned to. */
	SENT_CLIENT_INFORMATION,

	/* They've sent their username/password, and it's been accepted.  All they can 
	 * do from here is join a channel.  This can also occur if they've left all 
	 * channels. */
	NOT_IN_CHANNEL,

	/* They've joined a channel, and they are now ready to go */
	JOINED_CHANNEL

} user_states_t;

typedef struct
{
	int socket;

	char username[MAX_NAME];
	user_states_t state;
	int client_token;
	int server_token;
	char *room;

	char ip[IP_LENGTH];
	
} user_t;

/* Create a new, empty user.  They start in state CONNECTED, with a NULL username, 
 * a blank client token, and a random server token */
user_t *create_user(int socket, char *ip);
/* Clean up the user */
void destroy_user(user_t *user);

/* Get the user's socket */
int get_socket(user_t *user);

/* Set the username for the user, this should happen after they've authenticated */
void set_username(user_t *user, char *username);
/* Retrieve the username for the user */
char *get_username(user_t *user);
/* Retrieve the ip for the user */
char *get_ip(user_t *user);
/* Set the state for the user.  This module doesn't care what the state is, so make 
 * sure it's a valid transition */
void set_user_state(user_t *user, user_states_t new_state);
/* Get the state for this user */
user_states_t get_user_state(user_t *user);
/* Turn the user state into a string.  This string may NOT be modified! */
const char *get_user_state_string(user_t *user);
/* Set the client token.  This is received when the user sends his information */
void set_client_token(user_t *user, uint32_t token);
/* Get the client token that was previously set */
uint32_t get_client_token(user_t *user);
/* Get the server token, this is a random value generated when a user instance is
 * created */
uint32_t get_server_token(user_t *user);

/* The name of the current chatroom, to save me a lot of time.
 * WARNING: returns a pointer to a static string, don't muck around with it  */
char *get_user_room(user_t *user);
/* Set the name of hte current room */
void set_user_room(user_t *user, char *room_name);

/* Display the user; for debugging */
void print_user(user_t *user);


#endif


