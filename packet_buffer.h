/* packet_buffer */
/* This is a fairly generic buffer for sending packets.  This is based on the
 * Battle.net protocol.  All packets are in the form:
 * (int8) 0xFF -- For alignment.
 * (int8) code -- The code for the packet (ie, the request type)
 * (int16) length -- The length of the total packet, including the header, in 
 *  little endian form (most significant byte last)
 */

#include <stdint.h>
#include <unistd.h>

#include "types.h"

#ifndef _PACKET_BUFFER_H_
#define _PACKET_BUFFER_H_

/* Define a reasonable upper size for packet (anything longer than this should be shunned */
#define MAX_PACKET 9600

/* This struct shouldn't be accessed directly */
typedef struct 
{
	/* The current position in the string, used when reading it. */
	uint16_t position;
	/* The maximum length of the buffer that "buffer" is pointing to.  When 
	 * space in this runs out, it's expanded  */
	uint16_t max_length;
	/* The current buffer.  Will always point to a string of length max_length */
	uint8_t *data;
	/* Set to FALSE when the packet is destroyed, to make sure I don't accidentally
	 * re-use it (again) */
	BOOLEAN valid;

} packet_buffer_t;


/* Create a new packet buffer */
packet_buffer_t *create_buffer(uint8_t code);

/* Create a new packet buffer, with data.  The data shouldn't include the packet header, 
 * it will be added.  The length is the length of the data, without the header. */
packet_buffer_t *create_buffer_data(uint8_t code, uint16_t length, void *data);

/* Destroy the buffer and free resources.  If this isn't used, memory will leak. */
void destroy_buffer(packet_buffer_t *buffer);

/* Add data to the end of the buffer */
packet_buffer_t *add_int8(packet_buffer_t *buffer, uint8_t data);
packet_buffer_t *add_int16(packet_buffer_t *buffer, uint16_t data);
packet_buffer_t *add_int32(packet_buffer_t *buffer, uint32_t data);
packet_buffer_t *add_ntstring(packet_buffer_t *buffer, char *data);
packet_buffer_t *add_bytes(packet_buffer_t *buffer, void *data, uint16_t length);

/* Read the next data from the buffer.  The first read will be at the beginning (immediately
 * after the 4 header bytes).  An assertion will fail and the program will end if read off
 * the end of the buffer; it's probably a good idea to verify that enough data can be removed
 * before actually attempting to remove it; otherwise, a DoS condition can occur */
uint8_t read_next_int8(packet_buffer_t *buffer);
uint16_t read_next_int16(packet_buffer_t *buffer);
uint32_t read_next_int32(packet_buffer_t *buffer);
char *read_next_ntstring(packet_buffer_t *buffer, char *data_ret, uint16_t max_length);
void *read_next_bytes(packet_buffer_t *buffer, void *data, uint16_t length);

/* These boolean functions check if there's enough bytes left in the buffer to remove
 * specified data.  These should always be used on the server side to verify valid
 * packets */
BOOLEAN can_read_int8(packet_buffer_t *buffer);
BOOLEAN can_read_int16(packet_buffer_t *buffer);
BOOLEAN can_read_int32(packet_buffer_t *buffer);
BOOLEAN can_read_ntstring(packet_buffer_t *buffer);
BOOLEAN can_read_bytes(packet_buffer_t *buffer, uint16_t length);

/* Print out the buffer in a nice format */
void print_buffer(packet_buffer_t *buffer);

/* Get the code for the buffer */
uint8_t get_code(packet_buffer_t *buffer);

/* Get the total length of the buffer (including the header) */
uint16_t get_length(packet_buffer_t *buffer);

/* Returns a pointer to the actual buffer (including the header) */
uint8_t *get_buffer(packet_buffer_t *buffer);

/* Receives a full packet from the given socket.  Returns the new buffer if successful. 
 * If NULL is returned, there was a receive error that was already handled.  
 * If -1 is returned, the socket is dead and should not be used again. 
 * Don't forget to free it! */
packet_buffer_t *read_buffer(int s);

/* Sends the full packet over the given socket, and returns the number of bytes
 * sent (see write(2) for return values. */
ssize_t send_buffer(packet_buffer_t *buffer, int s);


#endif
