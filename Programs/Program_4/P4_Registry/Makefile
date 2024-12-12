# EECE-446-FA-2024 | Nick Kaplan | Halin Gailey

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g

# Target executable name
TARGET = registry

# Default target to build the program
all: $(TARGET)

# Rule to compile the executable from registry.c
$(TARGET): registry.c
	$(CC) $(CFLAGS) -o $(TARGET) registry.c

# Clean up the generated files
clean:
	rm -f $(TARGET)