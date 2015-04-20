WARN=-pedantic -Wall -Wshadow -Wpointer-arith -Wcast-qual \
	-Wstrict-prototypes 
CFLAGS=-g $(WARN) -std=c99
CINCFLAGS=
LDFLAGS=
CC=gcc
TARGET=cmoo
SUBDIRS=

SOURCES=$(shell ls *.c)
OBJECTS=$(subst .c,.o,$(SOURCES))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo "Linking $@..."
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) $(CINCFLAGS) -c $<
	@$(CC) -MM $(CFLAGS) $(CINCFLAGS) -c $< > $*.d

clean:
	@echo "Cleaning..."
	@rm -f *.o *.d
	@rm -f $(TARGET)

-include *.d
