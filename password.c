/* password */
/* This module simply hashes passwords. */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "password.h"

/* Out has to be a buffer with the size HASH_LENGTH */
char *password_hash_once(char *password, uint8_t *out)
{
	SHA_CTX context;
	SHA1_Init(&context);
	SHA1_Update(&context, password, strlen(password));
	SHA1_Final(out, &context);
	return out;
}

/* Hash the string twice.  out has to be a buffer with size HASH_LENGTH */
char *password_hash_twice(char *password, uint32_t client_token, uint32_t server_token, uint8_t *out)
{
	uint8_t temp_buf[HASH_LENGTH];
	return password_hash_second(password_hash_once(password, temp_buf), client_token, server_token, out);
}

/* Hash the string for the second time.  The first parameter has to be a hash, so it's
 * HASH_LENGTH bytes, and out has to be a buffer of HASH_LENGTH bytes long */
char *password_hash_second(uint8_t *first_hash, uint32_t client_token, uint32_t server_token, uint8_t *out)
{
	uint8_t *temp_buffer;
	int i;

	/* This is tricky, because we have to hash client_token and server_token in Little-Endian style */
	temp_buffer = malloc(HASH_LENGTH + 8);

	temp_buffer[0] = (client_token & 0x000000FF) >> 0;
	temp_buffer[1] = (client_token & 0x0000FF00) >> 8;
	temp_buffer[2] = (client_token & 0x00FF0000) >> 16;
	temp_buffer[3] = (client_token & 0xFF000000) >> 24;

	temp_buffer[4] = (server_token & 0x000000FF) >> 0;
	temp_buffer[5] = (server_token & 0x0000FF00) >> 8;
	temp_buffer[6] = (server_token & 0x00FF0000) >> 16;
	temp_buffer[7] = (server_token & 0xFF000000) >> 24;

	for(i = 0; i < HASH_LENGTH; i++)
		temp_buffer[8 + i] = first_hash[i];

	SHA1(temp_buffer, HASH_LENGTH + 8, out);

	free(temp_buffer);

	return out;
}


