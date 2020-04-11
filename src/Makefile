WARN=-Wall -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes 
# XXX _GNU_SOURCE is for recursive pthread locks, which we may not need for too long
CFLAGS=-g $(WARN) -std=c11 -pthread -D_GNU_SOURCE
CINCFLAGS=
LDFLAGS=-lev
CC=gcc
TARGET=cmoo
SUBDIRS=test

SOURCES=$(shell ls *.c)
OBJECTS=$(subst .c,.o,$(SOURCES))

.PHONY: all clean test check $(SUBDIRS)

all: $(TARGET) $(SUBDIRS)

$(SUBDIRS):
	@$(MAKE) -C $@

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
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

test: check
check: $(OBJECTS) $(SUBDIRS)
	$(MAKE) -C test check

-include *.d
