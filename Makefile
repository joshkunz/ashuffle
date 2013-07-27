.PHONY: clean

CFLAGS = -std=c99 -Wall -Wextra -pedantic -Werror 
libs = libmpdclient 

# For library linkage
CFLAGS += $(shell pkg-config $(libs) --cflags)
LDLIBS = $(shell pkg-config $(libs) --libs)

ashuffle:

clean: 
	-rm ashuffle
