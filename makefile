# Use the gcc compiler.
CC = gcc

# Flags for the compiler.
CFLAGS = -Wall -Wextra -Werror -std=c99 -pedantic

# Command to remove files.
RM = rm -f

# Phony targets - targets that are not files but commands to be executed by make.
.PHONY: all default clean run_tcp_server runtc runts runuc runus

# Default target - compile everything and create the executables and libraries.
all: TCP_Receiver TCP_Sender 

# Alias for the default target.
default: all


############
# Programs #
############

# Compile the tcp server.
TCP_Receiver: TCP_Receiver.o
	$(CC) $(CFLAGS) -o $@ $^

# Compile the tcp client.
TCP_Sender: TCP_Sender.o
	$(CC) $(CFLAGS) -o $@ $^

# Compile the udp server.



################
# Run programs #
################

# Run tcp server.
runtr: TCP_Receiver
	./TCP_Receiver -p 56469 -algo cubic &

# Run tcp client.
runts: TCP_Sender
	./TCP_Sender -p 56469 -algo cubic



################
# System Trace #
################

# Run the tcp Receiver with system trace.
runtr_trace: TCP_Receiver
	strace ./TCP_Receiver

# Run the tcp Sender with system trace.
runts_trace: TCP_Sender
	strace ./TCP_Sender


################
# Object files #
################

# Compile all the C files into object files.
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


#################
# Cleanup files #
#################

# Remove all the object files, shared libraries and executables.
clean:
	$(RM) *.o *.so TCP_Receiver TCP_Sender 