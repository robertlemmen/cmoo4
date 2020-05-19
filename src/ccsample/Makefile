WARN=-Wall -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wno-unused-function
# XXX _GNU_SOURCE is for recursive pthread locks, which we may not need for too long
CFLAGS=-g $(WARN) -std=c11 -pthread -D_GNU_SOURCE -I..
CINCFLAGS=
LDFLAGS=../types.o
CC=gcc
FLEX=flex
BISON=bison
TREECC=treecc
TARGET=cmc
SUBDIRS=test

SOURCES=$(shell ls *.c)
OBJECTS=$(subst .c,.o,$(SOURCES))

$(TARGET): cc_lex.o cc_par.o cc_ast.o $(OBJECTS)
	@echo "Linking $@..."
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

cmc.o: cc_par.h cc_lex.h cc_ast.h
main.o: cc_ast.h
cc_lex.o: cc_par.h
cc_par.o: cc_lex.h
cc_ast.o: cc_ast.c

%.o: %.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) $(CINCFLAGS) -c $<
	@$(CC) -MM $(CFLAGS) $(CINCFLAGS) -c $< > $*.d


cc_par.h: cc_par.c
cc_par.c: cc.y
	@echo "Generating $@..."
	@$(BISON) --defines=cc_par.h -o$@ -v $<

cc_lex.h: cc_lex.c
cc_lex.c: cc.l 
	@echo "Generating $@..."
	@$(FLEX) -o$@ $<

cc_ast.h: cc_ast.c
cc_ast.c: cc.tc
	@echo "Generating $@..."
	@$(TREECC) -o$@ $<

clean:
	@echo "Cleaning..."
	@rm -f $(TARGET)
	@rm -f *.o *.d
	@rm -f cc_par.c cc_par.h cc_lex.h cc_lex.c cc_ast.h cc_ast.c cc_par.output
