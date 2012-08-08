/* password */
/* This module simply hashes passwords. */

#ifndef _PASSWORD_H_
#define _PASSWORD_H_

#include <stdint.h>
#include <openssl/sha.h>

/* The number of bytes in the hashes */
#define HASH_LENGTH SHA_DIGEST_LENGTH

/* Out has to be a buffer with the size HASH_LENGTH */
char *password_hash_once(char *password, uint8_t *out);

/* Hash the string twice.  out has to be a buffer with size HASH_LENGTH */
char *password_hash_twice(char *password, uint32_t client_token, uint32_t server_token, uint8_t *out);

/* Hash the string for the second time.  The first parameter has to be a hash, so it's
 * HASH_LENGTH bytes, and out has to be a buffer of HASH_LENGTH bytes long */
char *password_hash_second(uint8_t *first_hash, uint32_t client_token, uint32_t server_token, uint8_t *out);

#endif

