CC=gcc 

# LIBS=-lssl -lcrypto -lsocket -lnsl -lcurses
LIBS=-lssl -lcurses
CFLAGS=-Wall -ansi -std=c89 -g -D_POSIX_SOURCE

all: server client
	@echo "Server and Client should be compiled"

install:
	@echo "This is just a homework assignment; no installation"

clean:
	rm -f server client *.o core
	# Test files:
	rm -f packet_buffer table account

client: client.o output.o packet_buffer.o user.o password.o table.o
	@echo "***** COMPILING CLIENT *****"
	${CC} ${CFLAGS} ${LIBS} -o client client.o output.o packet_buffer.o user.o password.o table.o

server: server.o output.o user.o list.o table.o packet_buffer.o password.o account.o room.o
	@echo "***** COMPILING SERVER *****"
	${CC} ${CFLAGS} ${LIBS} -o server user.o server.o output.o list.o table.o packet_buffer.o password.o account.o room.o

nc: nc.o output.o user.o
	${CC} ${CFLAGS} ${LIBS} -o nc nc.o output.o user.o

#client: client.o output.o
#	${CC} ${CFLAGS} -o client client.o output.o

# These are for testing, they'll eventually be removed
#account: account.o password.o 
#	${CC} ${CFLAGS} ${LIBS} -o account account.o password.o 

