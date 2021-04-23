CFLAGS = -O3 -Wall
all: extractlinks parselinks reader1 tree1

extractlinks: extractlinks.o printlinks.o
	$(CC) $^ -lxml2 $(CFLAGS) -o $@

parselinks: parselinks.o buffer.o
	$(CC) $^ $(CFLAGS) -o $@

reader1: reader1.c
	$(CC) $^ -lxml2 $(CFLAGS) -o $@

tree1: tree1.c
	$(CC) $^ -lxml2 $(CFLAGS) -o $@

%.o: %.c
	$(CC) -c $< $(CFLAGS) -o $@

clean:
	rm -f extractlinks reader1 tree1 *.o
