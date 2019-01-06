.PHONY: libtap

libtap = libtap

LDFLAGS += -static -L $(libtap)
LDLIBS += -ltap
CFLAGS += -std=c99 -Wall -Wextra -pedantic
CFLAGS += -I $(libtap) -I ../

libtap:
	make -C $(libtap)

../%.o:
	make -C .. $*.o

list_test: list_test.c ../list.o
