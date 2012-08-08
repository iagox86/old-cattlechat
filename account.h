/* account */
/* This module contains information about each logged in account.  The information
 * is loaded from a configuration file, and contains information such as their
 * accountname, password, current channel, and state.  It may eventually be expanded
 * to include the last time they were seen online, how long they've spend online, 
 * etc., but that's only if I have free time. 
 *
 * This information should be stored in a configuration file in the home directory, 
 * but since this is a school project, it's going to store it in the current
 * directory.  
 */

#ifndef _ACCOUNT_H
#define _ACCOUNT_H

#define MIN_NAME 2
#define MAX_NAME 32

#include <stdint.h>

#include "password.h"

typedef enum
{
	CREATE_SUCCESS,
	NAME_TOO_SHORT, /* The accountname was shorter than MIN_NAME */
	NAME_TOO_LONG, /* The accountname was longer than MAX_NAME */
	NAME_ILLEGAL, /* The name contained illegal characters */
	ACCOUNT_EXISTS /* The account already exists */
} create_response_t;

typedef enum
{
	LOGIN_SUCCESS,
	INCORRECT_PASSWORD,
	UNKNOWN_ACCOUNT,
	ACCOUNT_IN_USE
} login_response_t;

/* Log in.  The password can be calculated as, H(client_token . server_token . H(password)).  The tokens are 
 * random values, used to disuade brute-forcing, and H is the hash function (probably SHA1).  If the login 
 * failed, NULL is returned. */
login_response_t account_login(char *accountname, uint8_t password[HASH_LENGTH], uint32_t client_token, uint32_t server_token);

/* Create a new account account.  The given password is the hash of the actual password. */
create_response_t account_create(char* accountname, uint8_t password[HASH_LENGTH]);

#endif

