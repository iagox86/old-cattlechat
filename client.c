#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <netinet/in.h>

#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include "account.h"
#include "output.h"
#include "packet_buffer.h"
#include "room.h"
#include "types.h"

uint32_t client_token;
uint32_t server_token;

#define MAX_STRING 256


char username[MAX_STRING];
char password[MAX_STRING];
char channel[MAX_STRING];

/* Send out an error packet to the specified user, with the specified error text. */
void send_error(int s, char *error_text)
{
	packet_buffer_t *packet = create_buffer(SID_ERROR);
	add_ntstring(packet, error_text);
	send_buffer(packet, s);
	destroy_buffer(packet);
}

void send_chat(char *command, int s)
{
	packet_buffer_t *out;

	out = create_buffer(SID_CHATCOMMAND);
	add_ntstring(out, command);

	send_buffer(out, s);
	destroy_buffer(out);
}

void process_SID_NULL(packet_buffer_t *packet, int s)
{
	/* display_message(ERROR_DEBUG, "Received keep-alive"); */
}

void process_SID_SERVER_INFORMATION(packet_buffer_t *packet, int s)
{
	char *string_buffer = malloc(get_length(packet));
	packet_buffer_t *response;
	uint8_t password_hash[HASH_LENGTH];

	/* (uint32_t) server_token -- Used when hashing the password
	 * (uint32_t) version_useable -- If this is 0, this version is too old to connect and
	 *   has to be updated. 
	 * (ntstring) authentication hash type (should be "sha1")
	 * (ntstring) country -- This can be blank
	 * (ntstring) operating_system -- This can be blank */
	server_token = read_next_int32(packet);
	read_next_int32(packet);
	read_next_ntstring(packet, string_buffer, get_length(packet) - 1);
	read_next_ntstring(packet, string_buffer, get_length(packet) - 1);
	read_next_ntstring(packet, string_buffer, get_length(packet) - 1);

	set_display_header("Received server information");

	display_message(ERROR_NOTICE, "Received server information; attempting to log in");

	
	/* (uint32_t[5]) password -- Hash of the client token, server token, and password's hash.  If a 
	 *  hash is used that is less than 160-bit, it's padded with anything.  If a hash is used
	 *  that's longer than 160-bit, it's truncated.  The formula is H(ct, st, H(pass)).
	 * (ntstring) username */
	response = create_buffer(SID_LOGIN);
	password_hash_twice(password, client_token, server_token, password_hash);
	add_bytes(response, password_hash, 20);
	add_ntstring(response, username);
	send_buffer(response, s);
	destroy_buffer(response);

	free(string_buffer);
}

void process_SID_LOGIN_RESPONSE(packet_buffer_t *packet, int s)
{
	login_response_t result;
	char *buffer;

	packet_buffer_t *create;
	uint8_t create_password_buffer[20];

	result = read_next_int32(packet);

	switch(result)
	{
		case LOGIN_SUCCESS:

			buffer = malloc(get_length(packet) + 5);

			display_message(ERROR_NOTICE, "Logged in successfully; attempting to join channel '%s'", channel);
			set_display_header("Log in successful");

			strcpy(buffer, "/join ");
			strcat(buffer, channel);
			send_chat(buffer, s);

			free(buffer);

			break;
	
		case INCORRECT_PASSWORD:
			set_display_header("Incorrect password");
			display_error(ERROR_NOTICE, "Password was incorrect");
			break;
	
		case UNKNOWN_ACCOUNT:
			set_display_header("Creating account");
			display_message(ERROR_NOTICE, "Account not found, attempting to create it");
			password_hash_once(password, create_password_buffer);

	/* (uint32_t[5]) password -- Hash of the client's password only, without the tokens.  For 
	 *  information on storing hashes of different sizes, see SID_LOGIN
	 * (ntstring) username */
			create = create_buffer(SID_CREATE);
			add_bytes(create, create_password_buffer, 20);
			add_ntstring(create, username);
			send_buffer(create, s);
			destroy_buffer(create);
			
			break;

		case ACCOUNT_IN_USE:
			set_display_header("Account already in use");
			display_error(ERROR_NOTICE, "Account is already in use by somebody else, please select another");
			break;
	
		default:
			display_error(ERROR_ERROR, "Unknown login result code: %d", result);
	}

}

void process_SID_CREATE_RESPONSE(packet_buffer_t *packet, int s)
{
	create_response_t result;
	char *username_buffer = malloc(get_length(packet));
	packet_buffer_t *response;
	uint8_t password_hash[HASH_LENGTH];


	/* (uint32_t) result -- see create_response_t in account.h for result codes. 
	 * (ntstring) username */
	result = read_next_int32(packet);
	read_next_ntstring(packet, username_buffer, get_length(packet) - 1);

	switch(result)
	{
		case CREATE_SUCCESS:
			set_display_header("New account created");
			display_message(ERROR_NOTICE, "Account '%s' successfully created!", username_buffer);

	/* (uint32_t[5]) password -- Hash of the client token, server token, and password's hash.  If a 
	 *  hash is used that is less than 160-bit, it's padded with anything.  If a hash is used
	 *  that's longer than 160-bit, it's truncated.  The formula is H(ct, st, H(pass)).
	 * (ntstring) username */
			response = create_buffer(SID_LOGIN);
			password_hash_twice(password, client_token, server_token, password_hash);
			add_bytes(response, password_hash, 20);
			add_ntstring(response, username);
			send_buffer(response, s);
			destroy_buffer(response);

			break;

		case NAME_TOO_SHORT:
			set_display_header("Account too short");
			display_error(ERROR_CRITICAL, "The account you selected was too short.  Please select a longer one.");
			break;

		case NAME_TOO_LONG:
			set_display_header("Account too long");
			display_error(ERROR_CRITICAL, "The account you selected was too long.  Please select a shorter one.");
			break;

		case NAME_ILLEGAL:
			set_display_header("Account illegal");
			display_error(ERROR_CRITICAL, "The account you selected was illegal.  Please select a new one.");
			break;

		case ACCOUNT_EXISTS:
			set_display_header("Account exists");
			display_error(ERROR_CRITICAL, "The name you selected is already in use.  Please select a different one.");
			break;

		default:
			display_error(ERROR_CRITICAL, "Unknown CREATE_RESPONSE code: %d", result);
	}

	free(username_buffer);
}

void process_SID_ROOM_LIST(packet_buffer_t *packet, int s)
{
	send_error(s, "SID_ROOM_LIST Not implemented yet..");
}

void process_SID_CHATEVENT(packet_buffer_t *packet, int s)
{
	chatevent_subtype_t subtype;
	char *username_buffer = malloc(get_length(packet));
	char *text_buffer = malloc(get_length(packet));
	
	/* (uint32_t) subtype -- the subtype of the event.  See the enum below for a list.
	 * (ntstring) username  -- The username of the person who caused the event, if 
	 *  applicable
	 * (ntstring) text -- The text of the event, if applicable */
	subtype = read_next_int32(packet);
	read_next_ntstring(packet, username_buffer, get_length(packet) - 1);
	read_next_ntstring(packet, text_buffer, get_length(packet) - 1);

	display_channel_event(subtype, username_buffer, text_buffer, NULL, !strcmp(username_buffer, username));

	free(username_buffer);
	free(text_buffer);
}

/* Connect to the remote server.  If this returns, the connection was successful. */
int do_connect(char *host, int port)
{
	struct sockaddr_in serv_addr;
	struct hostent *server;

	/* Create the socket */
	int s = socket(AF_INET, SOCK_STREAM, 0);

	if (s < 0) 
		display_error(ERROR_EMERGENCY, "Error opening socket [%s]", strerror(errno));

	/* Look up the host */
	server = gethostbyname(host);
	if (server == NULL) 
		display_error(ERROR_EMERGENCY, "Couldn't find host %s [%s]", host, strerror(errno));

	/* Set up the server address */ 
	memset(&serv_addr, '\0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
	serv_addr.sin_port = htons(port);

	/* Connect */
	if (connect(s, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) 
		display_error(ERROR_EMERGENCY, "Error connecting to host %s [%s]", host, strerror(errno));

	display_message(ERROR_DEBUG, "Connection to %s successful!", host);	

	return s;
}

BOOLEAN process_next_packet(int s)
{
	packet_buffer_t *packet;
	char *string_buffer;

	packet = read_buffer(s);

	if(packet == (packet_buffer_t *) -1)
	{
		display_error(ERROR_EMERGENCY, "Connection closed [%s]", strerror(errno));
		return FALSE;
	}

	/* Allocate room for storing strings */
	string_buffer = malloc(get_length(packet));

	/* This is the heart of the packet process */
	switch(get_code(packet))
	{
		case SID_NULL:
			process_SID_NULL(packet, s);
			break;
		/* Server -> Client packets (We want to process these) */
		case SID_SERVER_INFORMATION:
			process_SID_SERVER_INFORMATION(packet, s);
			break;
		case SID_LOGIN_RESPONSE:
			process_SID_LOGIN_RESPONSE(packet, s);
			break;
		case SID_CREATE_RESPONSE:
			process_SID_CREATE_RESPONSE(packet, s);
			break;
		case SID_ROOM_LIST:
			process_SID_ROOM_LIST(packet, s);
			break;
		case SID_CHATEVENT:
			process_SID_CHATEVENT(packet, s);
			break;


		/* This packet can go either way */
		case SID_ERROR:
			display_message(ERROR_ERROR, "Server sent an error; message was, '%s'", read_next_ntstring(packet, string_buffer, get_length(packet)));
			break;

		/* Client -> Server packets (We shouldn't get these */ 
		case SID_CLIENT_INFORMATION:
		case SID_LOGIN:
		case SID_CREATE:
		case SID_REQUEST_ROOM_LIST:
		case SID_CHATCOMMAND:

			send_error(s, "Server isn't allowed to send that");
			break;

		default:
			send_error(s, "You sent an unknown packet!");
	}

	free(string_buffer);
	destroy_buffer(packet);

	return TRUE;
}

BOOLEAN do_select(int s)
{
	fd_set select_set;
	int select_ret;
	char *typed_string;

	/* We need to wait on stdin and s */
	FD_SET(s, &select_set);
	FD_SET(fileno(stdin), &select_set);

	/* Do the select */
	select_ret = select(s + 1, &select_set, NULL, NULL, NULL);

	if(select_ret == 0)
	{
		/* TODO: timeout */
	}
	else if(select_ret == -1)
	{
		display_error(ERROR_CRITICAL, "Select failed [%s]", strerror(errno));
	}
	else
	{
		if(FD_ISSET(fileno(stdin), &select_set))
		{
			/* They typed a character; let the interface process it */
			typed_string = read_next();

			if(typed_string)
				send_chat(typed_string, s);
		}

		/* Data has arrived on the socket */
		if(FD_ISSET(s, &select_set))
		{
			return process_next_packet(s);
		}
	}

	/* If we made it here, life is good */
	return TRUE;
}

/* This will read the next string, up to the length "buffer", and remove the newline at the 
 * end.  The string will always be termiinated.  The maximum length is MAX_STRING */
void read_string(char *buffer, char *default_value, BOOLEAN hide)
{
	int i;
	struct termios initialsettings, newsettings;

	if(hide)
	{
		tcgetattr(fileno(stdin), &initialsettings);
		newsettings = initialsettings;
		newsettings.c_lflag &= ~ECHO;
		tcsetattr(fileno(stdin), TCSAFLUSH, &newsettings);
	}

	fgets(buffer, MAX_STRING - 1, stdin);

	if(hide)
	{
		printf("\n");
		tcsetattr(fileno(stdin), TCSANOW, &initialsettings);
	}

	buffer[MAX_STRING - 1] = '\0';

	for(i = 0; i < strlen(buffer); i++)
		if(buffer[i] == '\r' || buffer[i] == '\n')
			buffer[i] = '\0';

	if(strlen(buffer) == 0)
		strncpy(buffer, default_value, MAX_STRING - 1);
}

int main(int argc, char *argv[])
{
	int s;
	packet_buffer_t *packet;

	char hostname[MAX_STRING];
	char port[MAX_STRING];

	srand(time(NULL));

	printf("Hostname [localhost] --> ");
	read_string(hostname, "localhost", FALSE);
	printf("Port [1024] --> ");
	read_string(port, "1024", FALSE);
	printf("Username [test] --> ");
	read_string(username, "test", FALSE);
	printf("Password [password] --> ");
	read_string(password, "password", TRUE);
	printf("Channel [My Channel] --> ");
	read_string(channel, "My Channel", FALSE);

	initialize_display();

	s = do_connect(hostname, atoi(port));

	packet = create_buffer(SID_CLIENT_INFORMATION);
	 /* (uint32_t) client_token -- Used when hashing the password
	 * (uint32_t) current_time -- Used to calculate time differences, if that ever comes up 
	 * (uint32_t) client_version -- Could be used for upgrades and stuff
	 * (ntstring) country -- Could be used for localized information.  Can be blank. 
	 * (ntstring) operating_system -- Why not?  Can be blank. */
	add_int32(packet, client_token = rand());
	add_int32(packet, time(NULL));
	add_int32(packet, 0);
	add_ntstring(packet, "Canada");
	add_ntstring(packet, "Linux");
	display_message(ERROR_NOTICE, "Sending client information");
	send_buffer(packet, s);
	destroy_buffer(packet);


	while(do_select(s))
		;
		

	destroy_display();
	return 0;
}
