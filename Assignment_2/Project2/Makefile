# Directory paths
SRCDIR := src
INCDIR := include
OBJDIR := obj
BINDIR := bin

# Compiler and flags
CC := gcc
CCFLAGS  := -Wall -g -c -DDEBUG=0 -O3
INCLUDES := -I$(INCDIR)
LIBS := -lrt -lpthread

# Target executables
CLIENT := $(BINDIR)/object/tinyfile-client
SERVER := $(BINDIR)/object/tinyfile-server

# Object files
CLIENT_OBJ := $(OBJDIR)/client.o
SERVER_OBJ := $(OBJDIR)/tinyfile_server.o
SNAPPY_OBJ := $(OBJDIR)/snappy.o

# Phony targets
.PHONY: all client server clean

# Default target to build everything
all: client server

# Build the client
client: $(CLIENT)

$(CLIENT): $(CLIENT_OBJ) | $(BINDIR)
	$(CC) $^ -o $@ $(LIBS)

# Build the server
server: $(SERVER)

$(SERVER): $(SERVER_OBJ) $(SNAPPY_OBJ) | $(BINDIR)
	$(CC) $^ -o $@ $(LIBS)

# Compile object files
$(CLIENT_OBJ): $(SRCDIR)/client/client.c $(INCDIR)/tinyfile/client.h $(INCDIR)/tinyfile/common_api.h | $(OBJDIR)
	$(CC) $(CCFLAGS) $(INCLUDES) -c $< -o $@

$(SERVER_OBJ): $(SRCDIR)/server/server.c $(INCDIR)/tinyfile/server.h $(INCDIR)/tinyfile/common_api.h | $(OBJDIR)
	$(CC) $(CCFLAGS) $(INCLUDES) -c $< -o $@

$(SNAPPY_OBJ): include/snappy-c/snappy.c include/snappy-c/snappy.h | $(OBJDIR)
	$(CC) $(CCFLAGS) -Iinclude -c $< -o $@

# Ensure directories exist
$(BINDIR):
	mkdir -p $@

$(OBJDIR):
	mkdir -p $@

# Clean build artifacts
clean:
	rm -f $(OBJDIR)/*.o
	rm -f $(BINDIR)/object/*
