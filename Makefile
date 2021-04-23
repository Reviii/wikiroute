CFLAGS = -O3 -Wall
all: extractlinks parselinks

extractlinks: extractlinks.o printlinks.o
	$(CC) $^ -lxml2 $(CFLAGS) -o $@

parselinks: parselinks.o buffer.o
	$(CC) $^ $(CFLAGS) -o $@

%.o: %.c
	$(CC) -c $< $(CFLAGS) -o $@

links.txt: wikidump.bz2 extractlinks
	bzcat wikidump.bz2 | ./extractlinks - | LC_ALL=C sort >links.txt

clean:
	rm -f extractlinks parselinks *.o
