#ifndef _TYPES_H_
#define _TYPES_H_

#define DEBUG_MODE

#ifndef TRUE
typedef enum 
{ 
	FALSE, 
	TRUE 
} BOOLEAN;
#else
typedef int BOOLEAN;
#endif

#define PROGRAM "Cattle Chat"
#define VERSION "v1.0"

typedef enum
{
	/* This is used as a keepalive packet.  
	 * Structure:
	 * (void) */
	SID_NULL,

	/* The initial packet sent to the server.
	 * Structure:
	 * (uint32_t) client_token -- Used when hashing the password
	 * (uint32_t) current_time -- Used to calculate time differences, if that ever comes up 
	 * (uint32_t) client_version -- Could be used for upgrades and stuff
	 * (ntstring) country -- Could be used for localized information.  Can be blank. 
	 * (ntstring) operating_system -- Why not?  Can be blank. */
	SID_CLIENT_INFORMATION,

	/* The server returns this after receiving SID_CLIENT_INFORMATION
	 * Structure:
	 * (uint32_t) server_token -- Used when hashing the password
	 * (uint32_t) version_useable -- If this is 0, this version is too old to connect and
	 *   has to be updated. 
	 * (ntstring) authentication hash type (should be "sha1")
	 * (ntstring) country -- This can be blank
	 * (ntstring) operating_system -- This can be blank */
	SID_SERVER_INFORMATION,

	/* The user attempts to log in.  This can be done after receiving SERVER_INFORMATION, or 
	 * after a failed login, or after a create.
	 * Structure:
	 * (uint32_t[5]) password -- Hash of the client token, server token, and password's hash.  If a 
	 *  hash is used that is less than 160-bit, it's padded with anything.  If a hash is used
	 *  that's longer than 160-bit, it's truncated.  The formula is H(ct, st, H(pass)).
	 * (ntstring) username */
	SID_LOGIN, 

	/* The response to the SID_LOGIN packet. If this was successful, a room can be joined. 
	 * Structure:
	 * (uint32_t) result -- see login_response_t in account.h for result codes.  
	 * (ntstring) username
	 */
	
	SID_LOGIN_RESPONSE,
	/* The user attempts to create an account.  This can be sent after SID_CLIENT_INFORMATION, 
	 * and before the user finishes logging in.  
	 * Structure:
	 * (uint32_t[5]) password -- Hash of the client's password only, without the tokens.  For 
	 *  information on storing hashes of different sizes, see SID_LOGIN 
	 * (ntstring) username */
	SID_CREATE,

	/* The response to the create packet.  If this was successful, the user can go on to log in
	 * with the chosen account. 
	 * Structure:
	 * (uint32_t) result -- see create_response_t in account.h for result codes. 
	 * (ntstring) username */
	SID_CREATE_RESPONSE,

	/* This requests a list of who is in a specified room.  This can be sent by anybody at 
	 * any time, including without authentication.  It can also be sent via the datagram 
	 * socket. 
	 * Structure:
	 * (ntstring) room_name */
	SID_REQUEST_ROOM_LIST,

	/* The response to SID_REQUEST_ROOM_LIST
	 * Structure:
	 * (ntstring[]) channels -- An array of user names who are in the channel, terminated by
	 *  a blank username. */
	SID_ROOM_LIST,

	/* Send an outgoing command in chat. 
	 * Structure:
	 * (ntstring) command -- A plaintext command, typed by the user. */
	SID_CHATCOMMAND,

	/* A received event in chat.  
	 * Structure:
	 * (uint32_t) subtype -- the subtype of the event.  See the enum below for a list.
	 * (ntstring) username  -- The username of the person who caused the event, if 
	 *  applicable
	 * (ntstring) text -- The text of the event, if applicable */
	SID_CHATEVENT,

	/* This is sent when an invalid packet is received, provided the packet is NOT a SID_ERROR. 
	 * If an unexpected SID_ERROR returned a SID_ERROR, we'd be in an infinite loop.  not good!
	 * Structure:
	 * (ntstring) description -- A description, in english, of what caused the error */
	SID_ERROR
	
} packet_codes_t;


typedef enum
{
	/* A user joined the channel you're currently in.  */
	EID_USER_JOIN_CHANNEL,

	/* A user is already in the channel that you just joined */
	EID_USER_IN_CHANNEL,

	/* A user left the channel that you're in */
	EID_USER_LEAVE_CHANNEL,

	/* The topic in the channel has changed */
	EID_TOPIC_CHANGED,

	/* An informational message */
	EID_INFO,

	/* A simple error message */
	EID_ERROR,

	/* A user talked */
	EID_TALK,

	/* The user joined a channel */
	EID_CHANNEL,

	/* An outgoing whisper message */
	EID_WHISPERTO,

	/* An incoming whisper message */
	EID_WHISPERFROM
} chatevent_subtype_t;

#endif

