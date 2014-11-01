CC=gcc
CFLAGS=-I.
DEPS = # header file 
OBJ = webserver.o 

%.o: %.c $(DEPS)
	$(CC) -g -c -o $@ $< $(CFLAGS)

webserver: $(OBJ)
	$(CC) -g -o $@ $^ $(CFLAGS)


