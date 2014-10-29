CC=gcc
CFLAGS=-I.
DEPS = # header file 
OBJ = serverFork.o 

%.o: %.c $(DEPS)
	$(CC) -g -c -o $@ $< $(CFLAGS)

serverFork: $(OBJ)
	$(CC) -g -o $@ $^ $(CFLAGS)


