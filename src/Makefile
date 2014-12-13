.PHONY: clean

QUIET_CC = @printf ' CC %b\n' $@;
CC = $(QUIET_CC)clang 

CFLAGS = -std=c99 -Wall -Wextra -pedantic
libs = libmpdclient

# For library linkage
CFLAGS += $(shell pkg-config $(libs) --cflags)
LDLIBS = $(shell pkg-config $(libs) --libs)

objects = list.o rule.o args.o shuffle.o

ashuffle: $(objects)
list.o: list.h
rule.o: rule.h list.h
args.o: args.h rule.h list.h
shuffle.o: shuffle.h list.h

gdb: ashuffle
	gdb ./ashuffle

clean: 
	-rm $(objects) ashuffle
