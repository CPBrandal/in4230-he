# Compiler and flags
CC = gcc
CFLAGS = -Wall -Werror -g

# Executable targets
TARGET = mipd ping_client ping_server

# Object files for each target
OBJS_MIPD = mipd.o local_interfaces.o mip_arp.o pdu.o raw_socket.o ping.o utils.o
OBJS_CLIENT = ping_client.o ping.o pdu.o raw_socket.o mip_arp.o local_interfaces.o utils.o
OBJS_SERVER = ping_server.o ping.o pdu.o raw_socket.o mip_arp.o local_interfaces.o utils.o

# Rules to build the targets
all: $(TARGET)

# Build the MIP daemon
mipd: $(OBJS_MIPD)
	$(CC) $(CFLAGS) -o $@ $(OBJS_MIPD)

# Build the ping client
ping_client: $(OBJS_CLIENT)
	$(CC) $(CFLAGS) -o $@ $(OBJS_CLIENT)

# Build the ping server
ping_server: $(OBJS_SERVER)
	$(CC) $(CFLAGS) -o $@ $(OBJS_SERVER)

# Generic rule to compile .c files into .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f *.o $(TARGET)
