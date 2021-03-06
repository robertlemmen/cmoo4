WARN=-Wall -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes 
CFLAGS=-g $(WARN) -std=c11 -pthread -D_GNU_SOURCE
CINCFLAGS=$(shell pkg-config --cflags check) -I..
LDFLAGS=$(shell pkg-config --libs check)
CC=gcc
TARGET=cmoo_check

SOURCES=$(shell ls *.c)
OBJECTS=$(subst .c,.o,$(SOURCES))
TESTED_OBJECTS=../types.o ../eval.o ../object.o ../cache.o ../lobject.o ../store.o ../persist.o \
	../lobject.o ../lock.o

.PHONY: all clean check

all: $(TARGET)

$(TARGET): $(OBJECTS) $(TESTED_OBJECTS)
	@echo "Linking $@..."
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o :: ./%.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) $(CINCFLAGS) -c $<
	@$(CC) -MM $(CFLAGS) $(CINCFLAGS) -c $< > $*.d

../%.o :: ../%.c
	@echo "Compiling $<..."
	$(MAKE) -C .. $*.o

clean:
	@echo "Cleaning..."
	@rm -f *.o *.d
	@rm -f $(TARGET)

check: $(TARGET)
	@echo "Running Unit Tests..."
	@./$(TARGET)

-include *.d
