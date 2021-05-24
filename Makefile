CFLAGS = -O3 -Wall
ALL = extractlinks parselinks explore route
all: $(ALL)

extractlinks: extractlinks.o printlinks.o
	$(CC) $^ -lxml2 $(CFLAGS) -o $@

parselinks: parselinks.o buffer.o
	$(CC) $^ $(CFLAGS) -o $@

explore: explore.o buffer.o mapfile.o nodeutils.o
	$(CC) $^ $(CFLAGS) -o $@

route: route.o buffer.o mapfile.o nodeutils.o
	$(CC) $^ $(CFLAGS) -o $@

%.o: %.c %.h
	$(CC) -c $< $(CFLAGS) -o $@
%.o: %.c
	$(CC) -c $< $(CFLAGS) -o $@

links.txt: wikidump.bz2 extractlinks
	bzcat wikidump.bz2 | ./extractlinks - | LC_ALL=C sort -f >links.txt

nodes.bin: links.txt parselinks
	./parselinks links.txt nodes.bin titles.txt

clean:
	rm -f $(ALL) *.o
