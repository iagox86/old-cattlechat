/* packet_buffer */
/* This is a fairly generic buffer for sending packets.  This is based on the
 * Battle.net protocol.  All packets are in the form:
 * (int8) 0xFF -- For alignment.
 * (int8) code -- The code for the packet (ie, the request type)
 * (int16) length -- The length of the total packet, including the header, in 
 *  little endian form (most significant byte last)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "output.h"
#include "packet_buffer.h"
#include "types.h"

/* The initial max length of the string */
#define STARTING_LENGTH 64 

/* #define PRINT_PACKETS */

/* Set the header byte (should always be set to 0xFF) */
static void set_header(packet_buffer_t *buffer, uint8_t header)
{
	assert(buffer->valid);
	buffer->data[0] = header;
}

/* Set the code for the buffer */
static void set_code(packet_buffer_t *buffer, uint8_t code)
{
	assert(buffer->valid);
	buffer->data[1] = code;
}

/* Set the length of the buffer (careful!) */
static void set_length(packet_buffer_t *buffer, uint16_t length)
{
	assert(buffer->valid);
	buffer->data[2] = (length >> 0) & 0x00FF;
	buffer->data[3] = (length >> 8) & 0x00FF;
}
/* Add the specified length to the buffer */
static void increment_length(packet_buffer_t *buffer)
{
	assert(buffer->valid);
	set_length(buffer, get_length(buffer) + 1); 
}

/* Create a new packet buffer */
packet_buffer_t *create_buffer(uint8_t code)
{
	packet_buffer_t *new_buffer = malloc(sizeof(packet_buffer_t));
	assert(new_buffer);
	
	new_buffer->valid = TRUE;
	new_buffer->position = 4;
	new_buffer->max_length = STARTING_LENGTH;

	new_buffer->data = malloc(STARTING_LENGTH * sizeof(char));
	assert(new_buffer->data);
	set_header(new_buffer, (uint8_t)0xFF);
	set_code(new_buffer, code);
	set_length(new_buffer, 4);


	return new_buffer;
}

/* Create a new packet buffer, with data.  The data shouldn't include the packet header, 
 * it will be added.  The length is the length of the data, without the header. */
packet_buffer_t *create_buffer_data(uint8_t code, uint16_t length, void *data)
{
	packet_buffer_t *new_buffer = malloc(sizeof(packet_buffer_t));
	assert(new_buffer);

	new_buffer->valid = TRUE;
	new_buffer->position = 4;
	new_buffer->max_length = length + 4;
	new_buffer->data = malloc(new_buffer->max_length);
	assert(new_buffer->data);

	memcpy(new_buffer->data + 4, data, length);
	set_header(new_buffer, (uint8_t)0xFF);
	set_code(new_buffer, code);
	set_length(new_buffer, length + 4);


	return new_buffer;
}

/* Destroy the buffer and free resources.  If this isn't used, memory will leak. */
void destroy_buffer(packet_buffer_t *buffer)
{
	assert(buffer->valid);
	buffer->position = 0;
	buffer->max_length = 0;
	buffer->valid = FALSE;

	free(buffer->data);
	free(buffer);
}

/* Add data to the end of the buffer */
packet_buffer_t *add_int8(packet_buffer_t *buffer, uint8_t data)
{
	assert(buffer->valid);
	/* Check if we have enough room */
	if(get_length(buffer) == buffer->max_length)
	{
		/*fprintf(stderr, "Doubling length of string from %d to %d", buffer->max_length, buffer->max_length << 1); */
		/* Double the length */
		buffer->max_length <<= 1;
		/* Allocate the new data */
		buffer->data = realloc(buffer->data, buffer->max_length);

		assert(buffer->data); /* Out of memory */
	}

	buffer->data[get_length(buffer)] = data;
	increment_length(buffer);

	return buffer;
}
packet_buffer_t *add_int16(packet_buffer_t *buffer, uint16_t data)
{
	assert(buffer->valid);
	add_int8(buffer, (data >> 0) & 0x000000FF);
	add_int8(buffer, (data >> 8) & 0x000000FF);

	return buffer;
}
packet_buffer_t *add_int32(packet_buffer_t *buffer, uint32_t data)
{
	assert(buffer->valid);
	add_int8(buffer, (data >> 0) & 0x000000FF);
	add_int8(buffer, (data >> 8) & 0x000000FF);
	add_int8(buffer, (data >> 16) & 0x000000FF);
	add_int8(buffer, (data >> 24) & 0x000000FF);

	return buffer;
}
packet_buffer_t *add_ntstring(packet_buffer_t *buffer, char *data)
{
	int i;

	assert(buffer->valid);
	/* Adding +1 so it also adds the NULL-terminator */
	for(i = 0; i < strlen(data) + 1; i++)
		add_int8(buffer, data[i]);
	return buffer;
}
packet_buffer_t *add_bytes(packet_buffer_t *buffer, void *data, uint16_t length)
{
	int i;

	assert(buffer->valid);
	for(i = 0; i < length; i++)
		add_int8(buffer, ((uint8_t*)data)[i]);
	return buffer;
}

/* Read the next data from the buffer.  The first read will be at the beginning (immediately
 * after the 4 header bytes).  An assertion will fail and the program will end if read off
 * the end of the buffer.  it's probably a good idea to verify that enough data can be removed
 * before actually attempting to remove it; otherwise, a DoS condition can occur */
uint8_t read_next_int8(packet_buffer_t *buffer)
{
	assert(can_read_int8(buffer));
	assert(buffer->valid);
	return buffer->data[buffer->position++];
}
uint16_t read_next_int16(packet_buffer_t *buffer)
{
	uint16_t ret = 0;
	assert(buffer->valid);
	assert(can_read_int16(buffer));

	ret |= (read_next_int8(buffer) << 0)  & 0x000000FF;
	ret |= (read_next_int8(buffer) << 8)  & 0x0000FF00;

	return ret;
}
uint32_t read_next_int32(packet_buffer_t *buffer)
{
	uint32_t ret = 0;
	assert(buffer->valid);
	assert(can_read_int32(buffer));

	ret |= (read_next_int8(buffer) << 0)  & 0x000000FF;
	ret |= (read_next_int8(buffer) << 8)  & 0x0000FF00;
	ret |= (read_next_int8(buffer) << 16) & 0x00FF0000;
	ret |= (read_next_int8(buffer) << 24) & 0xFF000000;

	return ret;
}
char *read_next_ntstring(packet_buffer_t *buffer, char *data_ret, uint16_t max_length)
{
	uint8_t next = 0xFF;
	uint16_t i;
	assert(buffer->valid);
	assert(can_read_ntstring(buffer));

	for(i = 0; i < max_length - 1 && next != '\0'; i++)
	{
		next = read_next_int8(buffer);
		/* Filter out control characters here -- allow '\0' or any standard ascii */
		data_ret[i] = (next == '\0' || (next >= 0x20 && next <= 0x7F)) ? next : '.';
	}
	data_ret[i] = '\0';

	return data_ret;
}
void *read_next_bytes(packet_buffer_t *buffer, void *data, uint16_t length)
{
	uint16_t i;
	assert(buffer->valid);
	assert(can_read_bytes(buffer, length));

	for(i = 0; i < length; i++)
		((char*)data)[i] = read_next_int8(buffer);
	
	return data; 
}

/* These boolean functions check if there's enough bytes left in the buffer to remove
 * specified data.  These should always be used on the server side to verify valid
 * packets */
BOOLEAN can_read_int8(packet_buffer_t *buffer)
{
	assert(buffer->valid);
	return (buffer->position + 1 <= get_length(buffer));
}
BOOLEAN can_read_int16(packet_buffer_t *buffer)
{
	assert(buffer->valid);
	return (buffer->position + 2 <= get_length(buffer));
}
BOOLEAN can_read_int32(packet_buffer_t *buffer)
{
	assert(buffer->valid);
	return (buffer->position + 4 <= get_length(buffer));
}
BOOLEAN can_read_ntstring(packet_buffer_t *buffer)
{
	int i;
	assert(buffer->valid);

	for(i = buffer->position; i < get_length(buffer); i++)
		if(buffer->data[i] == '\0')
			return TRUE;

	return FALSE;
}
BOOLEAN can_read_bytes(packet_buffer_t *buffer, uint16_t length)
{
	assert(buffer->valid);
	return (buffer->position + length < get_length(buffer));
}

static char get_character_from_byte(uint8_t byte)
{
	if(byte < 0x20 || byte > 0x7F)
		return '.';
	return byte;
}

/* Print out the buffer in a nice format */
void print_buffer(packet_buffer_t *buffer)
{
	uint16_t length = get_length(buffer);
	uint16_t i, j;
	assert(buffer->valid);
	printf("Buffer contents:");
	for(i = 0; i < length; i++)
	{
		if(!(i % 16))
		{
			if(i > 0)
			{
				printf("   ");
				for(j = 16; j > 0; j--)
				{
					printf("%c", get_character_from_byte(buffer->data[i - j]));
				}
			}
			printf("\n%04X: ", i);
		}

		if(i == buffer->position)
			printf("%02X>", buffer->data[i]);
		else if(i == buffer->position - 1)
			printf("%02X<", buffer->data[i]);
		else
			printf("%02X ", buffer->data[i]);
	}

	for(i = length % 16; i < 17; i++)
		printf("   ");
	for(i = length - (length % 16); i < length; i++)
		printf("%c", get_character_from_byte(buffer->data[i]));

	printf("\nLength: 0x%X (%d)\n", length, length);
}

/* Get the code for the buffer */
uint8_t get_code(packet_buffer_t *buffer)
{
	assert(buffer->valid);
	return buffer->data[1];
}

/* Get the total length of the buffer (including the header) */
uint16_t get_length(packet_buffer_t *buffer)
{
	assert(buffer->valid);
	return (buffer->data[2]) | (buffer->data[3] << 8);
}

/* Returns a pointer to the actual buffer (including the header) */
uint8_t *get_buffer(packet_buffer_t *buffer)
{
	assert(buffer->valid);
	return buffer->data;
}

/* Receives a full packet from the given socket.  Returns the new buffer, NULL on a recoverable 
 * error, or -1 if the client should be disconnected. 
 * Don't forget to free it! */
packet_buffer_t *read_buffer(int s)
{
	uint8_t temp_byte;

	uint8_t header_byte;
	uint8_t code;
	uint16_t length;

	/* TODO: free variables on early returns */
	char *buf;
	char *string_buf;
	int amount;

	packet_buffer_t *return_buffer;

	if(read(s, &header_byte, 1) != 1)
		return (packet_buffer_t *)-1;
	while(header_byte != 0xFF)
	{
		display_message(ERROR_WARNING, "Discarding invalid header byte 0x%02x", header_byte);
		if(read(s, &header_byte, 1) != 1)
			return NULL;
	}
	
	/* TODO: in some cases, read might not return all the bytes at once, we might have to store and wait */
	if(read(s, &code, 1) != 1)
	{
		display_message(ERROR_ALERT, "Call to read() failed");
		return NULL;
	}

	/* Start reading the length */
	length = 0;
	if(read(s, &temp_byte, 1) != 1)
	{
		display_message(ERROR_ALERT, "Call to read() failed");
		return NULL;
	}
	/* We have the low-order byte of the length */
	length |= temp_byte;
	if(read(s, &temp_byte, 1) != 1)
	{
		display_message(ERROR_ALERT, "Call to read() failed");
		return NULL;
	}
	/* Now we have the high-order byte of the length */
	length |= (temp_byte << 8);

	/* display_message(ERROR_DEBUG, "Received a packet with the header 0x%02X, code 0x%02X, and length 0x%02X!", header_byte, code, length) */

	/* If they gave us a packet with a length field of less than 4, bad things can happen.  So just kill 
	 * anybody who does. */
	if(length < 4)
	{
		display_message(ERROR_ERROR, "Packet length was below 4 (either a software bug, or malicious intent...?)");
		return (packet_buffer_t *)-1;
	}
	else if(length > MAX_PACKET)
	{
		display_message(ERROR_ERROR, "Received a ridiculously long packet (%d bytes).. killing the connection.", length);
		return (packet_buffer_t *)-1;
	}

	length -= 4;

	buf = malloc(length);
	string_buf = malloc(length);

	amount = read(s, buf, length);

	/* TODO: This isn't a valid problem.  If they don't send the full packet at once, I should store and wait.  But
	 * that will be for later, if I have time. */
	if(amount != length)
	{
		display_message(ERROR_ALERT, "Call to read() failed.  The packet probably didn't arrive fully yet...");
		free(buf);
		free(string_buf);
		return NULL;
	}

	return_buffer = create_buffer_data(code, length, buf);
#ifdef PRINT_PACKETS
	printf("RECEIVED:\n");
	print_buffer(return_buffer);
#endif
	return return_buffer;
}

/* Sends the full packet over the given socket, and returns the number of bytes
 * sent (see write(2) for return values.  After this is called, the buffer is
 * destroyed, and shouldn't be used again. */
ssize_t send_buffer(packet_buffer_t *buffer, int s)
{
	assert(buffer->valid);

#ifdef PRINT_PACKETS
	printf("SENT:\n");
	print_buffer(buffer);
#endif

	return write(s, buffer->data, get_length(buffer));
}

/*
int main(int argc, char *argv[])
{
	packet_buffer_t *buf = create_buffer(0x0F);
	char data[16];

	add_ntstring(buf, "ntString #1");
	add_ntstring(buf, "ntString #2");
	add_ntstring(buf, "ntString #3");
	add_ntstring(buf, "This string is definitely longer than the 16 character limit...!");
	add_int8(buf, 0x01);
	add_int16(buf, 0x0302);
	add_int32(buf, 0x07060504);

	add_int32(buf, 0x0b0a0908);
	add_int32(buf, 0x0f0e0d0c);
	
	for( ; ; )
	{
		printf("\n\n%s\n", read_next_ntstring(buf, data, 16));
		print_buffer(buf);
	}
		
	destroy_buffer(buf);
	return 0;
} */

