/* server */
/* This is one of the main functions.  It will run the server, accept connections, and 
 * read from the sockets.  It will process the packet read, and call the appropriate 
 * function, then wait for more data.  
 * There will only ever be a single instance of server.  A single server can look after
 * multiple chat rooms.  
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include <netinet/in.h>

#include "list.h"
#include "output.h"
#include "packet_buffer.h"
#include "room.h"
#include "types.h"
#include "user.h"

#define KEEPALIVE 60

#define INPUT_LENGTH 1024

/* A list of users that haven't entered a channel yet.  Each element of this list is a user_t object. */
static list_t *new_users;

/* A list of all users on the server.  This is used for checking if somebody is already logged in.  A
   user is added to this list and removed from new_users as soon as he authenticates.  The 
   intersection of new_users and old_users is empty, and their union is all connected users in any
   state. */
static table_t *old_users;

/* The table of server rooms.  Each element in this list is a room_t. */
static table_t *rooms;

/* The socket that listens for connections.  This is module-level so I can close it when a signal 
 * is caught */
static int listen_socket;

static struct timeval select_timeout;



void open_socket(int port)
{
	struct sockaddr_in serv_addr;

	/* Get the server address */
	memset((char *) &serv_addr, '\0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);

	/* Create a socket */
	listen_socket = socket(AF_INET, SOCK_STREAM, 0);

	/* Bind the socket */
	if (bind(listen_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
		display_error(ERROR_EMERGENCY, "Error binding socket [%s]", strerror(errno));

	/* Switch the socket to listen mode */
	listen(listen_socket, 20);
}

/* Send out an error packet to the specified user, with the specified error text. */
static void send_error(user_t *user, char *error_text)
{
	packet_buffer_t *packet = create_buffer(SID_ERROR);

	add_ntstring(packet, error_text);

	send_buffer(packet, get_socket(user));
	destroy_buffer(packet);
}

/* Send a chat-style message to a particular user.  This can be a whisper, error, info, etc.
 * Returns FALSE if it fails (user wasn't found) */
static BOOLEAN send_chat(chatevent_subtype_t subtype, char *to, char *from, char *message)
{
	user_t *user;
	packet_buffer_t *packet;

	user = table_find(old_users, to);
	if(user == NULL)
		return FALSE;

	/* (uint32_t) subtype -- the subtype of the event
	 * (ntstring) username  -- The username of the person who caused the event, if 
	 *  applicable
	 * (ntstring) text -- The text of the event, if applicable */
	packet = create_buffer(SID_CHATEVENT);

	add_int32(packet, subtype);
	add_ntstring(packet, from);
	add_ntstring(packet, message);

	send_buffer(packet, get_socket(user));
	destroy_buffer(packet);

	return TRUE;	
}

/* Triggered by /rooms or /channels */
void process_command_rooms(user_t *user, char *param)
{
	int i;
	room_t **room_list;
	size_t num_rooms;
	char buffer[INPUT_LENGTH];

	if(strlen(param) > 0)
	{
		send_chat(EID_ERROR, get_username(user), get_username(user), "Usage: /rooms");
	}
	else
	{
		room_list = (room_t **) get_values(rooms, &num_rooms);
		
		send_chat(EID_INFO, get_username(user), get_username(user), "Here is the list of channels");

		for(i = 0; i < num_rooms; i++)
		{
			if(room_get_count(room_list[i]) > 0)
			{
				snprintf(buffer, INPUT_LENGTH - 1, "%s <%d users>", room_get_name(room_list[i]), room_get_count(room_list[i]));
				send_chat(EID_INFO, get_username(user), get_username(user), buffer);
			}
		}
	}
		
}

/* Triggered by /who, /list */
void process_command_who(user_t *user, char *param)
{
	int i;
	room_t *target;
	user_t **users;
	size_t user_count;
	char buffer[INPUT_LENGTH];

	if(strlen(param) == 0)
	{
		/* They didn't request a room */
		send_chat(EID_ERROR, get_username(user), get_username(user), "Usage: /who <room>");
	}
	else
	{
		/* Find the target room */
		target = table_find(rooms, param);
		/* Make sure the room exists */
		if(target == NULL)
		{
			send_chat(EID_ERROR, get_username(user), get_username(user), "Room not found.  If you were searching for a user, not a room, please use /whois <username>");
		}
		else
		{
			/* Get the list of users for the room */
			users = room_get_users(target, &user_count);

			if(user_count == 0)
			{
				/* This is a bit of a workaround; if the room has 0 users, that means that somebody at some point
				 * entered it, but they've since left it and it's empty.  Treat it as if it doesn't exist. */
				send_chat(EID_ERROR, get_username(user), get_username(user), "Room not found.  If you were searching for a user, not a room, please use /whois <username>");
			}
			else
			{
				/* Display the list */
				snprintf(buffer, INPUT_LENGTH - 1, "Users in room %s:", param);
				send_chat(EID_INFO, get_username(user), get_username(user), buffer);

				for(i = 0; i < user_count; i++)
				{
					snprintf(buffer, INPUT_LENGTH - 1, "%s <%s>", get_username(users[i]), get_ip(users[i]));
					send_chat(EID_INFO, get_username(user), get_username(user), buffer);
				}
				
			}

			free(users);
		}
	}
}

/* Triggered by /finger, /whois, or /whereis */
void process_command_finger(user_t *user, char *param)
{
	user_t *target;
	char buffer[INPUT_LENGTH];

	if(strlen(param) == 0)
	{
		send_chat(EID_ERROR, get_username(user), get_username(user), "Usage: /finger <user>");
	}
	else
	{
		target = table_find(old_users, param);

		if(target == NULL)
		{
			send_chat(EID_ERROR, get_username(user), get_username(user), "Sorry, that isn't isn't logged on");
		}
		else
		{
			snprintf(buffer, INPUT_LENGTH - 1, "User %s is connected from %s and is in the channel %s.", get_username(target), get_ip(target), get_user_room(target) == NULL ? "<not in chat>" : get_user_room(target));
			send_chat(EID_INFO, get_username(user), get_username(user), buffer);
		}
	}
}

/* Triggered by either /help, /h, /?*/
void process_command_help(user_t *user, char *param)
{
	if(strlen(param) == 0)
	{
		send_chat(EID_INFO, get_username(user), get_username(user), "Here is a list of some of the commands, maybe all:");
		send_chat(EID_INFO, get_username(user), get_username(user), "/help, /w, /join, /rooms, /who, /finger");
	}
	else if(!strcasecmp(param, "help") || !strcasecmp(param, "h") || !strcasecmp(param, "?"))
	{
		send_chat(EID_INFO, get_username(user), get_username(user), "Command: help");
		send_chat(EID_INFO, get_username(user), get_username(user), "Usage: /help [command]");
		send_chat(EID_INFO, get_username(user), get_username(user), "Aliases: /help, /h, /?");
		send_chat(EID_INFO, get_username(user), get_username(user), "If no command parameter is specified, /help displays the list of commands.  If a parameter is given, it will attempt to find help on the specified command and display it (much like this...).");
	}
	else if(!strcasecmp(param, "w") || !strcasecmp(param, "whisper") || !strcasecmp(param, "m") || !strcasecmp(param, "msg"))
	{
		send_chat(EID_INFO, get_username(user), get_username(user), "Command: w");
		send_chat(EID_INFO, get_username(user), get_username(user), "Usage: /w <user> <message>");
		send_chat(EID_INFO, get_username(user), get_username(user), "Aliases: /w, /whisper, /m, /msg");
		send_chat(EID_INFO, get_username(user), get_username(user), "Attempts to send the given message to the requested user.  The user can be anywhere, in or out of chat, as long as he is logged in.  If he's not logged in, an error is displayed.");
	}
	else if(!strcasecmp(param, "join") || !strcasecmp(param, "channel"))
	{
		send_chat(EID_INFO, get_username(user), get_username(user), "Command: join");
		send_chat(EID_INFO, get_username(user), get_username(user), "Usage: /join [channel]");
		send_chat(EID_INFO, get_username(user), get_username(user), "Aliases: /join, /channel");
		send_chat(EID_INFO, get_username(user), get_username(user), "If the channel parameter is given, it joins the specified channel.  The channel is created if it doesn't already exist.  If no parameter is given, it leaves chat.  This is create channel, join channel, and leave chat all rolled up into one.");
	}
	else if (!strcasecmp(param, "finger") || !strcasecmp(param, "whois") || !strcasecmp(param, "whereis"))
	{
		send_chat(EID_INFO, get_username(user), get_username(user), "Command: finger");
		send_chat(EID_INFO, get_username(user), get_username(user), "Usage: /finger <user>");
		send_chat(EID_INFO, get_username(user), get_username(user), "Aliases: /finger, /whois, /whereis");
		send_chat(EID_INFO, get_username(user), get_username(user), "Gets the ip and current location for the requested user.");
	}
	else if (!strcasecmp(param, "who") || !strcasecmp(param, "list"))
	{
		send_chat(EID_INFO, get_username(user), get_username(user), "Command: who");
		send_chat(EID_INFO, get_username(user), get_username(user), "Usage: /who <channel>");
		send_chat(EID_INFO, get_username(user), get_username(user), "Aliases: /who, /list");
		send_chat(EID_INFO, get_username(user), get_username(user), "Gets the username and ip for everybody in the requested channel.");
	}
	else if (!strcasecmp(param, "rooms") || !strcasecmp(param, "channels"))
	{
		send_chat(EID_INFO, get_username(user), get_username(user), "Command: rooms");
		send_chat(EID_INFO, get_username(user), get_username(user), "Usage: /rooms");
		send_chat(EID_INFO, get_username(user), get_username(user), "Aliases: /rooms, /channels");
		send_chat(EID_INFO, get_username(user), get_username(user), "Lists all rooms, and the number of users in each of them.");
	}
/* Triggered by /who, /list */
/* Triggered by /finger, /whois, or /whereis */
}

/* Triggered by either /w, /m, /whisper, /msg */
void process_command_w(user_t *user, char *param)
{
	user_t *to;
	char *message;

	message = strchr(param, ' ');

	if(message == NULL)
	{
		send_chat(EID_ERROR, get_username(user), get_username(user), "Usage: /w <user> <message>");
	}
	else
	{
		*message++ = '\0';

		to = table_find(old_users, param);
		if(to == NULL)
		{
			send_chat(EID_ERROR, get_username(user), get_username(user), "User not logged on");
		}
		else
		{
			send_chat(EID_WHISPERFROM, get_username(to),   get_username(user), message);
			send_chat(EID_WHISPERTO,   get_username(user), get_username(to),   message);
		}
	}
}

/* Triggered by either /join or /channel */
/* TODO: Add the command required by the assignment */
void process_command_join(user_t *user, char *param)
{
	char *old_room_name;
	room_t *old_room = NULL;
	room_t *room;

	if(!strcasecmp(param, "backstage"))
	{
		send_chat(EID_ERROR, get_username(user), get_username(user), "Sorry, that room is restricted");
		display_message(ERROR_ERROR, "User failed to join channel '%s': restricted", param);
	}
	else if(strlen(param) > 0 && strlen(param) < MIN_ROOM_LENGTH)
	{
		send_chat(EID_ERROR, get_username(user), get_username(user), "Sorry, the name of that room is too short");
		display_message(ERROR_ERROR, "User failed to join channel '%s': too short", param);
	}
	else if(strlen(param) >= MAX_ROOM_LENGTH)
	{
		send_chat(EID_ERROR, get_username(user), get_username(user), "Sorry, the name of that room is too long");
		display_message(ERROR_ERROR, "User failed to join channel '%s': too long", param);
	}
	else
	{
		/* Get the old room */
		old_room_name = get_user_room(user);
		if(old_room_name)
			old_room = table_find(rooms, old_room_name);

		/* Leave the old room */
		if(old_room)
		{
			room_remove_user(old_room, user);
			room_message(old_room, EID_USER_LEAVE_CHANNEL, get_username(user), "");
		}

		/* Check if they're leaving chat */
		if(strlen(param) == 0)
		{
			/* Notify the user */
			send_chat(EID_INFO, get_username(user), get_username(user), "Leaving chat");
			/* Notify the server */
			display_message(ERROR_NOTICE, "User %s has left chat", get_username(user), param);

			/* Tell them that they've went nowhere */
			send_chat(EID_CHANNEL, get_username(user), get_username(user), "");

			/* Set their state back to no channel (I don't think this is necessary, but whatever) */
			set_user_state(user, NOT_IN_CHANNEL);

			/* Move the user out of all rooms, officially */
			set_user_room(user, NULL);

		}
		else
		{
			/* Find the room in the room table */
			room = table_find(rooms, param);
			/* If the room doesn't already exist, create it and add it to the list */
			if(room == NULL)
			{
				send_chat(EID_INFO, get_username(user), get_username(user), "Creating new channel for you");
				display_message(ERROR_NOTICE, "Channel didn't exist, creating");
				room = room_create(param);
				table_add(rooms, param, room);

			}
	
		
			/* Respond with success */
			send_chat(EID_CHANNEL, get_username(user), get_username(user), param);
	
			/* Notify the server */
			display_message(ERROR_NOTICE, "User %s successfully joined channel '%s'", get_username(user), param);
	
			/* Send the list of users in the channel */
			room_send_users_in_channel(room, get_socket(user));
	
			/* Add the user to the room officially */
			set_user_state(user, JOINED_CHANNEL);
			set_user_room(user, param);

			room_add_user(room, user);
			room_message(room, EID_USER_JOIN_CHANNEL, get_username(user), "");
		}
	} 
}

void process_SID_REQUEST_ROOM_LIST(user_t *user, packet_buffer_t *packet)
{
	send_error(user, "SID_REQUEST_ROOM_LIST Not implemented yet..");
}

void process_SID_NULL(user_t *user, packet_buffer_t *packet)
{
}

void process_SID_CLIENT_INFORMATION(user_t *user, packet_buffer_t *packet)
{
	packet_buffer_t *response;
	char *string_buffer;
	 /* (uint32_t) client_token -- Used when hashing the password
	 * (uint32_t) current_time -- Used to calculate time differences, if that ever comes up 
	 * (uint32_t) client_version -- Could be used for upgrades and stuff
	 * (ntstring) country -- Could be used for localized information.  Can be blank. 
	 * (ntstring) operating_system -- Why not?  Can be blank. */

	if(get_user_state(user) != CONNECTED)
	{
		send_error(user, "SID_CLIENT_INFORMATION Invalid in this state");
	}
	else
	{
		string_buffer = malloc(get_length(packet));

		set_client_token(user, read_next_int32(packet));
		read_next_int32(packet);
		read_next_int32(packet);
		read_next_ntstring(packet, string_buffer, get_length(packet));
		read_next_ntstring(packet, string_buffer, get_length(packet));
	
		set_user_state(user, SENT_CLIENT_INFORMATION);

	/* (uint32_t) server_token -- Used when hashing the password
	 * (uint32_t) version_useable -- If this is 0, this version is too old to connect and
	 *   has to be updated. 
	 * (ntstring) authentication hash type (should be "sha1")
	 * (ntstring) country -- This can be blank
	 * (ntstring) operating_system -- This can be blank */
		response = create_buffer(SID_SERVER_INFORMATION);
		add_int32(response, get_server_token(user));
		add_int32(response, 1);
		add_ntstring(response, "sha1");
		add_ntstring(response, "");
		add_ntstring(response, "");
		send_buffer(response, get_socket(user));
		destroy_buffer(response);

		free(string_buffer);
	}

	destroy_buffer(packet);

}

void process_SID_LOGIN(user_t *user, packet_buffer_t *packet)
{
	/* (uint32_t[5]) password -- Hash of the client token, server token, and password's hash.  If a 
	 *  hash is used that is less than 160-bit, it's padded with anything.  If a hash is used
	 *  that's longer than 160-bit, it's truncated.  The formula is H(ct, st, H(pass)).
	 * (ntstring) username */
	uint8_t password_buffer[20];
	char *username_buffer;
	login_response_t status;
	packet_buffer_t *response;

	if(get_user_state(user) != SENT_CLIENT_INFORMATION)
	{
		send_error(user, "SID_LOGIN Invalid in this state");
	}
	else
	{
		username_buffer = malloc(get_length(packet));
		display_user_message(ERROR_NOTICE, user, "User attempted authentication");
		read_next_bytes(packet, password_buffer, 20);
		read_next_ntstring(packet, username_buffer, get_length(packet) - 1);

		/* Check if the username is already being used */
		if(table_find(old_users, username_buffer))
			status = ACCOUNT_IN_USE;
		else
			status = account_login(username_buffer, password_buffer, get_client_token(user), get_server_token(user));

	
		response = create_buffer(SID_LOGIN_RESPONSE);
	/* (uint32_t[5]) password -- Hash of the client's password only, without the tokens.  For 
	 *  information on storing hashes of different sizes, see SID_LOGIN */
		add_int32(response, status);
		add_ntstring(response, username_buffer);
		send_buffer(response, get_socket(user));
		destroy_buffer(response);
	
		if(status == LOGIN_SUCCESS)
		{
			/* The user successfully authenticated */
			set_username(user, username_buffer);
			display_message(ERROR_DEBUG, "User %s authenticated successfully!", get_username(user));

			/* Set the new state */
			set_user_state(user, NOT_IN_CHANNEL);

			/* Move him from the new_users list to the old_users table */
			list_remove_value(new_users, user);
			table_add(old_users, get_username(user), user);
		}
		else
		{
			display_user_message(ERROR_ERROR, user, "User failed authentication");
		}

		free(username_buffer);
	}
}

void process_SID_CREATE(user_t *user, packet_buffer_t *packet)
{
	uint8_t password_buffer[20];
	char *username_buffer;
	packet_buffer_t *response;
	create_response_t create_response;

	if(get_user_state(user) != SENT_CLIENT_INFORMATION)
	{
		send_error(user, "SID_CREATE Invalid in this state");
	}
	else
	{
		username_buffer = malloc(get_length(packet));
		/* (uint32_t[5]) password -- Hash of the client's password only, without the tokens.  For 
		 *  information on storing hashes of different sizes, see SID_LOGIN 
		 * (ntstring) username */
		read_next_bytes(packet, password_buffer, 20);
		read_next_ntstring(packet, username_buffer, get_length(packet) - 1);
	
		create_response = account_create(username_buffer, password_buffer);
	
	
		response = create_buffer(SID_CREATE_RESPONSE);
		/* (uint32_t) result -- see create_response_t in account.h for result codes. 
		 * (ntstring) username */
		add_int32(response, create_response);
		add_ntstring(response, username_buffer);
		send_buffer(response, get_socket(user));
		destroy_buffer(response);
	
		free(username_buffer);
	}
}

void process_SID_CHATCOMMAND(user_t *user, packet_buffer_t *packet)
{
	char *message;
	char *command;
	char *parameter;
	char *room;

	if(get_user_state(user) != JOINED_CHANNEL && get_user_state(user) != NOT_IN_CHANNEL)
	{
		send_error(user, "SID_CHATCOMMAND Invalid in this state");
	}
	else
	{
		/* Get the room they're in.  If the room is NULL, then they aren't in a room */
		room = get_user_room(user);

		/* Get the message they're sending */
		message = malloc(get_length(packet));
		read_next_ntstring(packet, message, get_length(packet));



		if(*message == '/')
		{
			/* Get to the actual message, past the initial / */
			command = message + 1;
			/* Get the parameter, "/command <parameter>" */
			parameter = strchr(command, ' ');
			/* Terminate the command if necessary */
			if(parameter)
				*parameter++ = '\0';
			else
				parameter = "";

			/* Do the appropriate action, or send an error to the user */
			if(!strcasecmp(command, "join") || !strcasecmp(command, "channel"))
			{
				process_command_join(user, parameter);
			}
			else if(!strcasecmp(command, "help") || !strcasecmp(command, "h") || !strcasecmp(command, "?"))
			{
				process_command_help(user, parameter);
			}
			else if(!strcasecmp(command, "finger") || !strcasecmp(command, "whois") || !strcasecmp(command, "whereis"))
			{
				process_command_finger(user, parameter);
			}
			else if(!strcasecmp(command, "w") || !strcasecmp(command, "whisper") || !strcasecmp(command, "m") || !strcasecmp(command, "msg"))
			{
				process_command_w(user, parameter);
			}
			else if (!strcasecmp(command, "who") || !strcasecmp(command, "list"))
			{
				process_command_who(user, parameter);
			}
			else if (!strcasecmp(command, "rooms") || !strcasecmp(command, "channels"))
			{
				process_command_rooms(user, parameter);
			}
			else
			{
				send_chat(EID_ERROR, get_username(user), get_username(user), "Unknown command; type /help for a command listing");
			}
		}
		else
		{
			if(room == NULL)
			{
				send_chat(EID_ERROR, get_username(user), get_username(user), "You can only send chat if you're in a room");
			}
			else
			{
				/* Distribute the message as a chat message */
				room_message(table_find(rooms, room), EID_TALK, get_username(user), message);
			}

		}

		free(message);
	}
}

void process_SID_ERROR(user_t *user, packet_buffer_t *packet)
{
	char *string_buffer = malloc(get_length(packet));
	display_user_message(ERROR_ERROR, user, "Client sent an error; message was, '%s'", read_next_ntstring(packet, string_buffer, get_length(packet)));
	free(string_buffer);
}


/* If everything goes well, return TRUE. 
 * If there's some error that can easily be handled, it handles it and returns TRUE
 * If there's some bad error, it prints the error message and returns FALSE.  If FALSE
 *  is returned, the socket should be closed and never used again. 
 */
BOOLEAN process_next_packet(user_t *user)
{
	packet_buffer_t *packet;

	int s = get_socket(user);

	packet = read_buffer(s);

	if(packet == NULL)
		return TRUE;
	else if(packet == (packet_buffer_t *) -1)
		return FALSE;


	/* This is the heart of the packet process */
	switch(get_code(packet))
	{
		case SID_NULL:
			process_SID_NULL(user, packet);
			break;

		/* Client -> Server packets (We process these) */ 
		case SID_CLIENT_INFORMATION:
			process_SID_CLIENT_INFORMATION(user, packet);
			break;

		case SID_LOGIN:
			process_SID_LOGIN(user, packet);
			break;

		case SID_CREATE:
			process_SID_CREATE(user, packet);
			break;

		case SID_REQUEST_ROOM_LIST:
			process_SID_REQUEST_ROOM_LIST(user, packet);
			break;

		case SID_CHATCOMMAND:
			process_SID_CHATCOMMAND(user, packet);
			break;

		/* This can go either way */
		case SID_ERROR:
			process_SID_ERROR(user, packet);
			break;


		/* Client -> Server packets (We shouldn't get these) */
		case SID_SERVER_INFORMATION:
		case SID_LOGIN_RESPONSE:
		case SID_CREATE_RESPONSE:
		case SID_ROOM_LIST:
		case SID_CHATEVENT:
			send_error(user, "Client isn't allowed to send that");
			break;

		default:
			send_error(user, "Unknown packet");
	}


	return TRUE;
}

/* Sends a keepalive to all clients, new and established */
void do_keepalive(user_t **new_user_list, int new_user_count, user_t **old_user_list, int old_user_count)
{
	int i;
	packet_buffer_t *keepalive;

	keepalive = create_buffer(SID_NULL);

	/* Send the keepalive to all the new users */
	for(i = 0; i < new_user_count; i++)
		send_buffer(keepalive, get_socket(new_user_list[i]));

	/* Send the keepalive to all the authenticated users */
	for(i = 0; i < old_user_count; i++)
		send_buffer(keepalive, get_socket(old_user_list[i]));

	destroy_buffer(keepalive);

}

void do_select()
{
	struct sockaddr_in client_address;
	int client_length = sizeof(client_address);
	int new_socket;
	fd_set select_set;
	int select_return;
	int i;
	int biggest_socket = listen_socket;

	user_t **new_user_list;
	int new_user_count;

	user_t **old_user_list;
	int old_user_count;

	/* Used as a temporary variable when a new connection is made */
	user_t *new_user;

	/* Clear the current socket set */
	FD_ZERO(&select_set);
	/* Add the listening socket for new connections */
	FD_SET(listen_socket, &select_set);

	/* Retrieve the list of new users */
	new_user_list = (user_t **) list_get_array(new_users, &new_user_count);
	for(i = 0; i < new_user_count; i++)
	{
		biggest_socket = (get_socket(new_user_list[i]) > biggest_socket) ? get_socket(new_user_list[i]) : biggest_socket;
		FD_SET(get_socket(new_user_list[i]), &select_set);
	}

	/* Retrieve the list of authenticated users */
	old_user_list = (user_t **) get_values(old_users, &old_user_count);
	for(i = 0; i < old_user_count; i++)
	{
		biggest_socket = (get_socket(old_user_list[i]) > biggest_socket) ? get_socket(old_user_list[i]) : biggest_socket;
		FD_SET(get_socket(old_user_list[i]), &select_set);
	}

	select_return = select(biggest_socket + 1, &select_set, NULL, NULL, &select_timeout);

	if(select_return == -1)
	{
		display_error(ERROR_EMERGENCY, "Select failed [%s]", strerror(errno));
	}
	else if(select_return == 0)
	{
		do_keepalive(new_user_list, new_user_count, old_user_list, old_user_count);

		select_timeout.tv_sec = KEEPALIVE;
		select_timeout.tv_usec = 0;
	}
	else
	{
		/* If the listen_socket is set, then we have a new connection */
		if(FD_ISSET(listen_socket, &select_set))
		{
			/* Accept the connection */
			new_socket = accept(listen_socket, (struct sockaddr *) &client_address, &client_length);
			/* Create a new user object */
			new_user = create_user(new_socket, inet_ntoa(client_address.sin_addr));
			/* Add the new user to the list of new users */
			list_add_end(new_users, new_user);
			/* Notify the user that there was a conection */
			display_message(ERROR_NOTICE, "Connection accepted from %s", get_ip(new_user));
			/* Make sure we don't accidentally user the new socket again */
			new_socket = (int)NULL;
		}

		/* Look after the old users first (because a new user might become an old user, but an old user
		 * will never become a new user, and doing new first might muck things up */
		for(i = 0; i < old_user_count; i++)
		{
			if(FD_ISSET(get_socket(old_user_list[i]), &select_set))
			{
				if(process_next_packet(old_user_list[i]) == FALSE)
				{
					display_message(ERROR_NOTICE, "Connection to socket %s [%s] closed", get_username(old_user_list[i]), get_ip(old_user_list[i]));
					table_remove(old_users, get_username(old_user_list[i]));
					close(get_socket(old_user_list[i]));
				}
			}
		}

		/* Look after the new users */
		for(i = 0; i < new_user_count; i++)
		{
			if(FD_ISSET(get_socket(new_user_list[i]), &select_set))
			{
				if(process_next_packet(new_user_list[i]) == FALSE)
				{
					display_message(ERROR_NOTICE, "Connection to %s closed", get_ip(new_user_list[i]));
					list_remove_value(new_users, new_user_list[i]);
					close(get_socket(new_user_list[i]));
				}
			}
		}
	}

	free(new_user_list);
	free(old_user_list);
}

/* This function will capture a variety of signals.  When any of them occurs, it will display
 * the fact that it happened, then clean up and exit cleanly. */
void die_gracefully(int signal)
{
	int i;

	user_t **new_user_list;
	int new_user_count;
	user_t **old_user_list;
	int old_user_count;

	display_message(ERROR_EMERGENCY, "Signal caught, we're gonna die.. closing sockets first");

	if(listen_socket)
		close(listen_socket);

	/* Retrieve the list of new users */
	new_user_list = (user_t **) list_get_array(new_users, &new_user_count);
	for(i = 0; i < new_user_count; i++)
		close(get_socket(new_user_list[i]));

	/* Retrieve the list of authenticated users */
	old_user_list = (user_t **) get_values(old_users, &old_user_count);
	for(i = 0; i < old_user_count; i++)
		close(get_socket(old_user_list[i]));

	display_message(ERROR_EMERGENCY, "Sockets closed, handling signal");

	switch(signal)
	{
		case SIGINT:
			display_error(ERROR_EMERGENCY, "Process was terminated by user");
			break;

		case SIGQUIT:
			display_error(ERROR_EMERGENCY, "Terminal quit");
			break;

		case SIGSEGV:
			display_message(ERROR_EMERGENCY, "Segmentation fault (aborting)");
			abort();
			break;

		case SIGTERM:
			display_error(ERROR_EMERGENCY, "Process was terminated");
			break;

		case SIGILL:
			display_message(ERROR_EMERGENCY, "Illegal instruction (something very bad happened) (aborting)");
			abort();
			break;

	}
}

int main(int argc, char *argv[])
{
	srand(time(NULL));
	initialize_display();
	set_display_header("SERVER");

	new_users = list_create();
	old_users = table_create();
	rooms = table_create();

	/* Initialize signals */
	signal(SIGINT, die_gracefully);
	signal(SIGQUIT, die_gracefully);
	signal(SIGSEGV, die_gracefully);
	signal(SIGTERM, die_gracefully);

	if (argc < 2) 
		display_error(ERROR_EMERGENCY, "Usage: %s <port>", argv[0]);

	display_message(ERROR_DEBUG, "Opening socket on port %s", argv[1]);
	open_socket(atoi(argv[1]));

	if (listen_socket < 0) 
		display_error(ERROR_EMERGENCY, "Error opening socket [%s]", strerror(errno));

	display_message(ERROR_DEBUG, "Socket opened on port %s", argv[1]);

	/* Set up the initial select_timeout */
	select_timeout.tv_sec = KEEPALIVE;
	select_timeout.tv_usec = 0;

	while(TRUE)
		do_select(listen_socket); 

	destroy_display();

	return 0;
}


