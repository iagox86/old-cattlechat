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

/* This is not a good implementation.  It reads through the entire file every time. 
 * But it will work, and can be replaced if there is time later. 
 * TODO: implement this better */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "account.h"
#include "types.h"
#include "password.h"

#define USERS_FILE "./accounts.ini"

#define MAX_RECORD (MAX_NAME + (HASH_LENGTH * 2) + 2)

typedef struct
{
	char accountname[MAX_NAME];
	uint8_t password[HASH_LENGTH];
} account_record;

/* Find the account, and fill in the record structure.  Return TRUE if they 
 * were found, and FALSE otherwise.  The record parameter can be NULL, in
 * which case it's not filled out. */
static BOOLEAN find_account(char *accountname, account_record *record)
{
	FILE *account_file = fopen(USERS_FILE, "r");
	char buffer[MAX_RECORD];
	char *delimit;
	int i;
	uint32_t temp_byte;
	/* This string holds a single byte (2 digits) */
	char temp_string[3];
	int line = 0;

	if(!account_file)
	{
		account_file = fopen(USERS_FILE, "a");
		if(!account_file)
		{
			perror("ERROR Failed to open accounts file");
			exit(1);
		}
		fclose(account_file);
		return FALSE;
	}

	while(fgets(buffer, MAX_RECORD - 1, account_file))
	{
		line++;
		buffer[MAX_RECORD - 1] = '\0';
		delimit = strchr(buffer, ';');

		if(delimit == NULL)
			continue;
		delimit[0] = '\0';

		/* Check if we've found the account */
		if(!strcmp(buffer, accountname))
		{
			/* If they didn't want the info, don't give it to them */
			if(record == NULL)
				return TRUE;

			/* Copy the accountname into the record data */
			strncpy(record->accountname, buffer, MAX_NAME - 1);
			record->accountname[MAX_NAME - 1] = '\0';
			/* Copy the 20-byte password */
			delimit++;
			/* Allow the hash string to be longer, because of end-line characters */
			if(strlen(delimit) < (HASH_LENGTH * 2))
			{
				printf("ERROR invalid hash on line %d:\n  <%s>\n  --> Invalid length (is %d, should be %d)\n", line, delimit, strlen(delimit), (HASH_LENGTH * 2));
				exit(1);
			}

			for(i = 0; i < HASH_LENGTH; i++)
			{
				strncpy(temp_string, delimit + (i * 2), 2);
				temp_string[2] = '\0';
				if(!(isxdigit(temp_string[0]) && isxdigit(temp_string[1])))
				{
					printf("ERROR invalid hash on line %d:\n  <%s>\n  --> Contains non-hex digit: 0x%02x\n", line, delimit, temp_string[0]);
					exit(1);
				}
				sscanf(temp_string, "%x", &temp_byte);
				record->password[i] = temp_byte & 0x00FF;
			}

			fclose(account_file);
			return TRUE; 
		}
	}

	fclose(account_file);
	return FALSE;
}

/* Assumes that the accountname and password are already validated, and adds them to the file */
static void add_account(char *accountname, uint8_t password[HASH_LENGTH])
{
	FILE *f;
	int i;

	/* Since this will break a lot, assert it. */
	assert(strchr(accountname, ';') == NULL);

	f = fopen(USERS_FILE, "a");
	if(!f)
	{
		perror("ERROR Failed to open accounts file for writing");
		exit(1);
	}

	fprintf(f, "%s;", accountname);
	for(i = 0; i < HASH_LENGTH; i++)
		fprintf(f, "%02x", password[i]);
	fprintf(f, "\n");
	fclose(f);
}

/* Log in.  The password can be calculated as, H(client_token . server_token . H(password)).  The tokens are 
 * random values, used to disuade brute-forcing, and H is the hash function (probably SHA1).  If the login 
 * failed, NULL is returned. */
login_response_t account_login(char *accountname, uint8_t password[HASH_LENGTH], uint32_t client_token, uint32_t server_token)
{
	int i;
	account_record record;
	uint8_t good_password[HASH_LENGTH];

	/* Check if the account exists */
	if(!find_account(accountname, &record))
		return UNKNOWN_ACCOUNT;

	/* Hash their password with the client and server tokens */
	password_hash_second(record.password, client_token, server_token, good_password);

	/* Compare the passwords */
	for(i = 0; i < HASH_LENGTH; i++)
		if(good_password[i] != password[i])
			return INCORRECT_PASSWORD;
	
	return LOGIN_SUCCESS;
}

/* Create a new account account.  The given password is the hash of the actual password. */
create_response_t account_create(char* accountname, uint8_t password[HASH_LENGTH])
{
	int i;

	/* Validate the accountname */
	if(strlen(accountname) < MIN_NAME)
		return NAME_TOO_SHORT;
	if(strlen(accountname) >= MAX_NAME)
		return NAME_TOO_LONG;
	for(i = 0; i < strlen(accountname); i++)
		if(!(isprint(accountname[i])) || accountname[i] == ';')
			return NAME_ILLEGAL;

	/* Check if the account already exists */	
	if(find_account(accountname, NULL))
		return ACCOUNT_EXISTS;

	/* Everything's good.  Add the account */
	add_account(accountname, password);

	return LOGIN_SUCCESS;
}

void test(uint32_t client_token, uint32_t server_token)
{
	uint8_t buf[HASH_LENGTH];

	password_hash_twice("password", client_token, server_token, buf);
	printf("Succeed: %d\n", account_login("test account...", buf, client_token, server_token));
	printf("Fail: %d\n", account_login("test account3", buf, client_token, server_token));
	printf("Doesn't exist: %d\n", account_login("test none", buf, client_token, server_token));

	password_hash_twice("password", server_token, client_token, buf);
	printf("Fail: %d\n", account_login("test account...", buf, client_token, server_token));
	printf("Fail: %d\n", account_login("test account3", buf, client_token, server_token));
	printf("Doesn't exist: %d\n", account_login("test none", buf, client_token, server_token));

}
/* login_response_t account_login(char *accountname, uint8_t password[HASH_LENGTH], uint32_t client_token, uint32_t server_token) */

/*int main(int argc, char *argv[])
{
	system("rm accounts.ini");

	printf("Succeed: %d\n", account_create("test account...", "password"));
	printf("Success: %d\n", account_create("test account3", "pwrd"));

	printf("Illegal: %d\n", account_create("test;account", "password"));
	printf("Illegal: %d\n", account_create("test account\n", "password"));
	printf("Short: %d\n", account_create("test account2", "prd"));
	printf("Exists: %d\n", account_create("test account...", "password"));

	test(0x12345678, 0x11223344);
	test(0xabcdef, 0);

	return 0;
}*/

