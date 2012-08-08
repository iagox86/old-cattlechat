/* output */
/* This module takes care of displaying server messages, including
 * errors or notifications.  It displays messages to stderr, and 
 * on certain ones (that are deemed fatal), kills the process */

#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <ncurses.h>

#include "types.h"
#include "user.h"

#define ERROR_MAX_LENGTH 1024
#define TIME_MAX_LENGTH 30

typedef enum 
{
	ERROR_NONE,       /* No error.  This is just to make life easier for me. */
	ERROR_DEBUG,      /* Debug-level messages */
	ERROR_INFO,       /* Informational */
	ERROR_NOTICE,     /* Normal but significant conditions */
	ERROR_WARNING,    /* Warning conditions */
	ERROR_ERROR,      /* Error conditions */
	ERROR_CRITICAL,   /* Critical conditions */
	ERROR_ALERT,      /* Action must be taken immediately */
	ERROR_EMERGENCY,  /* System is unusable */
} error_code_t;

/* This function will initialize ncurses, create the windows, get the colors ready, 
 * and any other graphical initialization */
void initialize_display();
/* Clean up all the graphical (ncurses) stuff */
void destroy_display();
/* Reads the next character from stdin.  If the string is done (terminated with a \n), 
 * it is returned.  Otherwise, NULL is returned.  NULL isn't an error, it's just an 
 * indication that the string isn't complete.  
 * WARNING: a pointer to a static buffer is returned, so this isn't thread-safe. */
char *read_next();

/* Set the text in the chanenl header (at the top-right) */
void set_display_channel(char *text);
/* Set the text in the header (at the top) */
void set_display_header(char *text);
/* A channel even occurred, display it.  For the server, we want to display the channel, so we set channel_name.  
 * If it's the client, set channel_name to NULL.  */
void display_channel_event(chatevent_subtype_t subtype, char *username, char *message, char *channel_name, BOOLEAN its_me);
/* A recoverable error or debug message has occured.  Display the message, and go on 
 * with our lives */
void display_message(error_code_t level, char *message, ...);
/* A recoverable error or debug message has occured.  Display the message in the specified ncurses
 * winow, and move on with our lives */
void wdisplay_message(error_code_t level, WINDOW *window, char *message, ...);
/* An unrecoverable error or debug condition (?) ha occurred.  Display the message, 
 * and exit with an error code */
void display_error(error_code_t level, char *message, ...);
/* An unrecoverable error or debug condition (?) ha occurred.  Display the message, 
 * in an ncurses window, then clean up ncurses and exit. */
void wdisplay_error(error_code_t level, WINDOW *window, char *message, ...);

/* A recoverable error that was caused by a user has occured.  Display the message and
 * information about the user (username, ip, state, etc.).  */
void display_user_message(error_code_t level, user_t *user, char *message, ...);

#endif

