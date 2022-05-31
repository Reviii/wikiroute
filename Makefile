CFLAGS = -O3 -Wall -Wextra -flto
ALL = extractlinks presort removeduplicates parselinks explore route parsesql
all: $(ALL)

extractlinks: extractlinks.o printlinks.o
	$(CC) $^ -lxml2 $(CFLAGS) -o $@

parselinks: parselinks.o buffer.o
	$(CC) $^ $(CFLAGS) -o $@

parsesql: parsesql.o buffer.o
	$(CC) $^ $(CFLAGS) -o $@

explore: explore.o buffer.o mapfile.o nodeutils.o
	$(CC) $^ $(CFLAGS) -o $@

route: route.o buffer.o mapfile.o nodeutils.o
	$(CC) $^ $(CFLAGS) -o $@

removeduplicates: removeduplicates.o buffer.o
	$(CC) $^ $(CFLAGS) -o $@

%.o: %.c %.h
	$(CC) -c $< $(CFLAGS) -o $@
%.o: %.c
	$(CC) -c $< $(CFLAGS) -o $@

links.txt: wikidump.bz2 extractlinks
	@echo Note: The progress bar only shows the progress of the first step
	pv wikidump.bz2 | bzcat | ./extractlinks - | ./presort | LC_ALL=C sort | cut -b 2- | ./removeduplicates >links.txt

nodes.bin: links.txt parselinks
	./parselinks links.txt nodes.bin titles.txt

clean:
	rm -f $(ALL) *.o
