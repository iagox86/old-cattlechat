/* output */
/* This module takes care of displaying server messages, including
 * errors or notifications.  It displays messages to stderr, and 
 * on certain ones (that are deemed fatal), kills the process */

#include <stdio.h>
#include <stdlib.h>

#include <ncurses.h>
#include <string.h>
#include <time.h>

#include "output.h"
#include "table.h"
#include "types.h"
#include "user.h"

#define MAX_MESSAGE 1024


static char *error_levels[] = { "", "DEBUG", "INFO", "NOTICE", "WARNING", "ERROR",  "CRITICAL", "ALERT", "EMERGENCY" };

static char read_buffer[MAX_MESSAGE];
static int read_location;

static table_t *user_list;

static WINDOW *header_outer;
static WINDOW *header_inner;

static WINDOW *channel_outer;
static WINDOW *channel_inner;

static WINDOW *chat_outer;
static WINDOW *chat_inner;

static WINDOW *input_outer;
static WINDOW *input_inner;

static WINDOW *list_inner;
static WINDOW *list_outer;

#define LIST_SIZE 24

/* Prototypes for static functions declares in this file */
static void update_userlist();
static void set_color(int color, BOOLEAN bold, BOOLEAN reverse);
static void set_error_color(error_code_t error);
static void reset_cursor();
static char *get_timestamp();
static void display_raw_message(int color, BOOLEAN bold, BOOLEAN timestamp, BOOLEAN endline, char *message, ...);

/* This function will initialize ncurses, create the windows, get the colors ready, 
 * and any other graphical initialization */
void initialize_display()
{
	initscr();
	start_color();

	init_pair(COLOR_BLACK,     COLOR_BLACK,    COLOR_BLACK);
	init_pair(COLOR_RED,       COLOR_RED,      COLOR_BLACK);
	init_pair(COLOR_GREEN,     COLOR_GREEN,    COLOR_BLACK);
	init_pair(COLOR_YELLOW,    COLOR_YELLOW,   COLOR_BLACK);
	init_pair(COLOR_BLUE,      COLOR_BLUE,     COLOR_BLACK);
	init_pair(COLOR_MAGENTA,   COLOR_MAGENTA,  COLOR_BLACK);
	init_pair(COLOR_CYAN,      COLOR_CYAN,     COLOR_BLACK);
	init_pair(COLOR_WHITE,     COLOR_WHITE,    COLOR_BLACK);

	user_list = table_create();

	/* The outer header window is just for the border */
	header_outer = newwin(3, COLS - LIST_SIZE, 0, 0);
	wborder(header_outer, 0, 0, 0, 0, 0, 0, 0, 0);
	wrefresh(header_outer);
	/* The inner header displays the important information */
	header_inner = newwin(1, COLS - 4 - LIST_SIZE, 1, 2);
	set_display_header("Not Connected");
	wrefresh(header_inner);

	/* The outer header window is just for the border */
	channel_outer = newwin(3, LIST_SIZE, 0, COLS - LIST_SIZE );
	wborder(channel_outer, 0, 0, 0, 0, 0, 0, 0, 0);
	wrefresh(channel_outer);
	/* The inner channel displays the important information */
	channel_inner = newwin(1, LIST_SIZE - 2, 1, COLS - LIST_SIZE + 1);
	set_display_channel("N/A");
	wrefresh(channel_inner);

	/* The outer chat window is just for the border */
	chat_outer = newwin(LINES - 7, COLS - LIST_SIZE, 3, 0);
	wborder(chat_outer, 0, 0, 0, 0, 0, 0, 0, 0);
	wrefresh(chat_outer);
	/* The inner chat window is where everything will be displayed, and is 
	 * scrollable. */
	chat_inner = newwin(LINES - 9, COLS - 2 - LIST_SIZE, 4, 1);
	scrollok(chat_inner, TRUE);

	/* This provides the border for the chat window */
	input_outer = newwin(3, COLS, LINES - 4, 0);
	wborder(input_outer, 0, 0, 0, 0, 0, 0, 0, 0);
	wrefresh(input_outer);
	/* This is where the user actually gets to type their input */
	input_inner = newwin(1, COLS - 2, LINES - 3, 1);
	wrefresh(input_inner);

	/* These will be the channel list at the right */
	list_outer = newwin(LINES - 7, LIST_SIZE, 3, COLS - LIST_SIZE);
	wborder(list_outer, 0, 0, 0, 0, 0, 0, 0, 0);
	wrefresh(list_outer);
	list_inner = newwin(LINES - 9, LIST_SIZE - 2, 4, COLS - LIST_SIZE + 1);
	wrefresh(list_inner);

	/* Move the cursor to the proper place */
	wmove(input_inner, 0, 0);
	/* I'm pretty sure this isn't necessary, but make sure it's clear */
	wclrtoeol(input_inner);

	read_location = 0;
	read_buffer[0] = '\0';

	/* Display some test data */
	display_message(ERROR_NONE,      "Test");
	display_message(ERROR_DEBUG,     "Test");
	display_message(ERROR_INFO,      "Test");
	display_message(ERROR_WARNING,   "Test");
	display_message(ERROR_NOTICE,    "Test");
	display_message(ERROR_ERROR,     "Test");
	display_message(ERROR_CRITICAL,  "Test");
	display_message(ERROR_ALERT,     "Test");
	display_message(ERROR_EMERGENCY, "Test");

	update_userlist();
}

static void update_userlist()
{
	char **list;
	size_t user_count;
	size_t i;

	list = (char**) get_keys(user_list, &user_count);

	wclear(list_inner);
	for(i = 0; i < user_count; i++)
		wprintw(list_inner, "%s\n", list[i]);
	wrefresh(list_inner);

	reset_cursor();

	free(list);
}

/* Disable any attributes, and enable a specific color/attributes */
static void set_color(int color, BOOLEAN bold, BOOLEAN reverse)
{
	int mask = 0;

	mask |= COLOR_PAIR(color);
	if(bold)
		mask |= A_BOLD;
	if(reverse)
		mask |= A_REVERSE;

	wattrset(chat_inner, mask);
}

/* Set the color/etc. for the specified error condition */
static void set_error_color(error_code_t error)
{
	switch(error)
	{
		case ERROR_NONE:
			set_color(COLOR_WHITE, TRUE, FALSE);
			break;

		case ERROR_DEBUG:
			set_color(COLOR_CYAN, FALSE, FALSE);
			break;

		case ERROR_INFO:
			set_color(COLOR_CYAN, TRUE, FALSE);
			break;

		case ERROR_NOTICE:
			set_color(COLOR_GREEN, FALSE, FALSE);
			break;

		case ERROR_WARNING:
			set_color(COLOR_GREEN, TRUE, FALSE);
			break;

		case ERROR_ERROR:
			set_color(COLOR_YELLOW, TRUE, FALSE);
			break;

		case ERROR_CRITICAL:
			set_color(COLOR_RED, FALSE, FALSE);
			break;

		case ERROR_ALERT:
		case ERROR_EMERGENCY:
			set_color(COLOR_RED, TRUE, FALSE);
	
			break;
	}
}

/* Clean up all the graphical (ncurses) stuff */
void destroy_display()
{
	endwin();
}

/* Put the cursor back at the end of the input.  This isn't required, but it looks 
 * nicer for the user.  Also, clear everything after the cursor.  */
static void reset_cursor()
{
	mvwprintw(input_inner, 0, 0, "%s", read_buffer);
	/* Note: have to keep this wmove for the cases where read_buffer is the wrong length
	 * (happens when they press enter */
	wmove(input_inner, 0, read_location);
	wclrtoeol(input_inner);
	wrefresh(input_inner);
}

/* Reads the next character from stdin.  If the string is done (terminated with a \n), 
 * it is returned.  Otherwise, NULL is returned.  NULL isn't an error, it's just an 
 * indication that the string isn't complete.  
 * WARNING: a pointer to a static buffer is returned, so this isn't thread-safe. */
char *read_next()
{
	char c;

	keypad(input_inner, TRUE);
	noecho(); /* Note to self: Is this necessary? */
	c = wgetch(input_inner);

	switch(c)
	{
		case '\n':

			read_location = 0;
			reset_cursor();

			return read_buffer;

		case 0x07:
		case 0x7F:
			if(read_location > 0)
			{
				read_location--;
				read_buffer[read_location] = '\0';
				reset_cursor();
			}

			break;

		default:

			read_buffer[read_location] = c;
			read_location++;
			read_buffer[read_location] = '\0';

			if(read_location >= MAX_MESSAGE)
			{
				/* Clear the input "box" */
				wmove(input_inner, 0, 0);
				wclrtoeol(input_inner);
				wrefresh(input_inner);
	
				read_location = 0;
	
				return read_buffer;
			}

			reset_cursor();
	}

	return NULL;
}

/* Set the text in the chanenl header (at the top-right) */
void set_display_channel(char *text)
{
	wmove(channel_inner, 0, 0);
	wclrtoeol(channel_inner);
	wprintw(channel_inner, "%s", text);
	wrefresh(channel_inner);
	reset_cursor();
}

/* Set the text in the header (at the top) */
void set_display_header(char *text)
{
	wmove(header_inner, 0, 0);
	wclrtoeol(header_inner);
	wprintw(header_inner, "%s %s -- %s", PROGRAM, VERSION, text);
	wrefresh(header_inner);
	reset_cursor();
}

/* A channel even occurred, display it.  For the server, we want to display the channel, so we set channel_name.  
 * If it's the client, set channel_name to NULL.  */
void display_channel_event(chatevent_subtype_t subtype, char *username, char *message, char *channel_name, BOOLEAN its_me)
{
/* static void set_color(int color, BOOLEAN bold, BOOLEAN reverse) */
	switch(subtype)
	{
		/* A user joined the channel you're currently in.  */
		case EID_USER_JOIN_CHANNEL:
			display_raw_message(COLOR_GREEN, TRUE, TRUE, TRUE, "%s has joined the channel", username);
			table_add(user_list, username, username);
			update_userlist();
			break;
	
		/* A user is already in the channel that you just joined */
		case EID_USER_IN_CHANNEL:
			display_raw_message(COLOR_GREEN, TRUE, TRUE, TRUE, "%s is in the channel", username);
			table_add(user_list, username, username);
			update_userlist();
			break;
	
		/* A user left the channel that you're in */
		case EID_USER_LEAVE_CHANNEL:
			display_raw_message(COLOR_GREEN, TRUE, TRUE, TRUE, "%s has left the channel", username);
			table_remove(user_list, username);
			update_userlist();
			break;
	
		/* The topic in the channel has changed */
		case EID_TOPIC_CHANGED:
			display_raw_message(COLOR_YELLOW, TRUE, TRUE, TRUE, "Channel topic is now %s", message);
			/* TODO: When I have an interface, change the topic there */
			break;
	
		/* An informational message */
		case EID_INFO:
			display_raw_message(COLOR_YELLOW, TRUE, TRUE, TRUE, "%s", message);
			break;
	
		/* A simple error message */
		case EID_ERROR:
			display_raw_message(COLOR_RED, TRUE, TRUE, TRUE, "%s", message);
			break;

		/* Somebody int he channel talked */
		case EID_TALK:
			display_raw_message(its_me ? COLOR_CYAN : COLOR_YELLOW, FALSE, TRUE, FALSE, "<%s> ", username);
			display_raw_message(COLOR_WHITE, TRUE, FALSE, TRUE, "%s", message);
			break;

		case EID_WHISPERFROM:
			display_raw_message(COLOR_WHITE, FALSE, TRUE, TRUE, "<From: %s> %s", username, message);
			break;

		case EID_WHISPERTO:
			display_raw_message(COLOR_WHITE, FALSE, TRUE, TRUE, "<To: %s> %s", username, message);
			break;

		/* Joined a new channel */
		/* TODO: Clear it, don't delete it */
		case EID_CHANNEL:
			table_destroy(user_list);
			user_list = table_create();
			update_userlist();

			table_add(user_list, username, username);
			if(strlen(message) > 0)
			{
				display_raw_message(COLOR_GREEN, TRUE, TRUE, TRUE, "Joining channel: %s", message);
				set_display_channel(message);
			}
			else
			{
				set_display_channel("N/A");
			}

			break;

		default:
			display_message(ERROR_ERROR, "Unknown CHATEVENT subtype: %d!", subtype);
	}

	wrefresh(chat_inner);
	reset_cursor();
}

/* NOT thread safe */
static char *get_timestamp()
{
	time_t time_val;
	static char time_str[20];
	struct tm *time_struct;

	time_val = time(NULL);
	time_struct = localtime(&time_val);	

	sprintf(time_str, "%d:%02d:%02d", time_struct->tm_hour, time_struct->tm_min, time_struct->tm_sec);

	return time_str;
}

/* A recoverable error or debug message has occured.  Display the message, and go on 
 * with our lives */
static void display_raw_message(int color, BOOLEAN bold, BOOLEAN timestamp, BOOLEAN endline, char *message, ...)
{
	char error_message[MAX_MESSAGE];
	va_list ap;

	/* Put the text into a string */
	va_start(ap, message);
	vsnprintf(error_message, MAX_MESSAGE - 1, message, ap);
	error_message[MAX_MESSAGE - 1] = '\0';
	va_end(ap);

	if(timestamp)
	{
		set_color(COLOR_WHITE, TRUE, FALSE);
		wprintw(chat_inner, "[%s] ", get_timestamp());
	}

	set_color(color, bold, FALSE);
	wprintw(chat_inner, "%s%s", error_message, endline ? "\n" : "");
		
	wrefresh(chat_inner);
	reset_cursor();
}
/* A recoverable error or debug message has occured.  Display the message, and go on 
 * with our lives */
void display_message(error_code_t level, char *message, ...)
{
	char error_message[MAX_MESSAGE];
	va_list ap;

	if(level > ERROR_EMERGENCY)
		level = ERROR_EMERGENCY;

	/* Put the text into a string */
	va_start(ap, message);
	vsnprintf(error_message, MAX_MESSAGE - 1, message, ap);
	error_message[MAX_MESSAGE - 1] = '\0';
	va_end(ap);

	set_color(COLOR_WHITE, TRUE, FALSE);
	wprintw(chat_inner, "[%s] ", get_timestamp());

	set_error_color(level);
	if(level == ERROR_NONE)
		wprintw(chat_inner, "%s\n", error_message);
	else
		wprintw(chat_inner, "[%s] %s\n", error_levels[level], error_message);
		
	wrefresh(chat_inner);
	reset_cursor();
}

/* An unrecoverable error or debug condition (?) ha occurred.  Display the message, 
 * and exit with an error code */
void display_error(error_code_t level, char *message, ...)
{
	char error_message[MAX_MESSAGE];
	va_list ap;

	if(level > ERROR_EMERGENCY)
		level = ERROR_EMERGENCY;

	/* Put the text into a string */
	va_start(ap, message);
	vsnprintf(error_message, MAX_MESSAGE - 1, message, ap);
	error_message[MAX_MESSAGE - 1] = '\0';
	va_end(ap);

	set_color(COLOR_WHITE, TRUE, FALSE);
	wprintw(chat_inner, "[%s] ", get_timestamp());

	set_error_color(level);
	if(level == ERROR_NONE)
		wprintw(chat_inner, "%s\n", error_message);
	else
		wprintw(chat_inner, "[%s] %s\n", error_levels[level], error_message);

	set_color(COLOR_RED, TRUE, FALSE);
	wprintw(chat_inner, "\nFATAL ERROR DETECTED\n\nPress any key to exit...\n\n\n");
	wrefresh(chat_inner);

	reset_cursor();
	wgetch(input_inner);
	
	destroy_display();
	exit(1);
}

/* A recoverable error that was caused by a user has occured.  Display the message and
 * information about the user (username, ip, state, etc.).  */
void display_user_message(error_code_t level, user_t *user, char *message, ...)
{
	char error_message[MAX_MESSAGE];
	va_list ap;

	if(level > ERROR_EMERGENCY)
		level = ERROR_EMERGENCY;

	/* Put the text into a string */
	va_start(ap, message);
	vsnprintf(error_message, MAX_MESSAGE - 1, message, ap);
	error_message[MAX_MESSAGE - 1] = '\0';
	va_end(ap);

	set_color(COLOR_WHITE, TRUE, FALSE);
	wprintw(chat_inner, "[%s] ", get_timestamp());

	set_error_color(level);
	if(level == ERROR_NONE)
		wprintw(chat_inner, "[%s] [%s {%s} %s]: %s\n", error_levels[level], get_username(user), get_user_state_string(user), get_ip(user), error_message);
	else
		wprintw(chat_inner, "[%s] [%s {%s} %s]: %s\n", error_levels[level], get_username(user), get_user_state_string(user), get_ip(user), error_message);
		
	wrefresh(chat_inner);
	reset_cursor();

}


/* int main(int argc, char *argv[])
{
	display_error(NOTICE, "This is a test error: %d should be 3!", 3);
	display_fatal_error(EMERGENCY, "This is a test error: %s should be 'hi!'!", "'hi!'");
	display_error(EMERGENCY, "This shouldn't even be displayed!", 3);

	return 0;
}*/


