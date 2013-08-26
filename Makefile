.PHONY: clean

CC = clang 

CFLAGS = -std=c99 -Wall -Wextra -pedantic
libs = libmpdclient 

# For library linkage
CFLAGS += $(shell pkg-config $(libs) --cflags)
LDLIBS = $(shell pkg-config $(libs) --libs)

objects = array.o list.o rule.o args.o shuffle.o

ashuffle: $(objects)
array.o: array.h
list.o: list.h
rule.o: rule.h array.h
args.o: args.h rule.h array.h
shuffle.o: shuffle.h list.h

gdb: ashuffle
	gdb ./ashuffle

clean: 
	-rm $(objects) ashuffle
