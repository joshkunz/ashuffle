.PHONY: clean

CFLAGS = -std=c99 -Wall -Wextra -pedantic
libs = libmpdclient 

# For library linkage
CFLAGS += $(shell pkg-config $(libs) --cflags)
LDLIBS = $(shell pkg-config $(libs) --libs)

objects = array.o rule.o

ashuffle: $(objects)
array.o: array.h
rule.o: rule.h array.h

clean: 
	-rm $(objects) ashuffle
