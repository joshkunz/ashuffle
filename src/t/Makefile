.PHONY: libtap

libtap = libtap

CC = clang
LD = clang
LDFLAGS += -L $(libtap)
LDLIBS += -ltap
CFLAGS += -std=c99 -Wall -Wextra -pedantic -Werror -g -O0
CFLAGS += -I $(libtap) -I ../
CFLAGS += -fsanitize=address -fsanitize=undefined

libtap:
	make -C $(libtap)

../%.o:
	make -C .. $*.o

list_test: list_test.c ../list.o
shuffle_test: shuffle_test.c ../list.o ../shuffle.o
