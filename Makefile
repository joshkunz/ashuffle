.PHONY: clean
libs = libmpdclient 

CFLAGS = -Wall -Wextra -pedantic -Werror $(shell pkg-config $(libs) --cflags)
LDLIBS = $(shell pkg-config $(libs) --libs)

ashuffle:

clean: 
	-rm ashuffle
