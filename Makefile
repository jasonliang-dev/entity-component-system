CC = gcc
CFLAGS = -std=c99 -Werror -Wall -Wextra -pedantic-errors -I.
DEPS = greatest.h ecs.h
OBJ = main.o ecs.o

all: $(OBJ)
	$(CC) -o ecs_test $^ $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: clean
clean:
	rm -f *.o ecs_test vgcore.*
