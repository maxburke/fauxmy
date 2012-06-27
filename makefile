CC = gcc
CFLAGS = -g -Wextra -Wall -pedantic -Werror
LDFLAGS = -g 
OBJ = main.o
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

fauxmy: $(OBJ)
	gcc -o $@ $^ $(LDFLAGS) $(LIBS)

.PHONY: clean
clean:
	rm -rf *.o fauxmy
