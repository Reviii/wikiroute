CFLAGS = -O3 -Wall -Wextra -flto -g
ALL = extractlinks printlinks-test removeduplicates presort parselinks verifynodes explore route parsepagelinks parsepagelinks2
all: $(ALL)

extractlinks: extractlinks.o printlinks.o
	$(CC) $^ -lxml2 $(CFLAGS) -o $@

parselinks: parselinks.o buffer.o
	$(CC) $^ $(CFLAGS) -o $@

parsepagelinks: parsepagelinks.o parsesql.o buffer.o
	$(CC) $^ $(CFLAGS) -o $@

parsepagelinks2: parsepagelinks2.o parsesql.o buffer.o
	$(CC) $^ $(CFLAGS) -o $@

verifynodes: verifynodes.o buffer.o mapfile.o nodeutils.o
	$(CC) $^ $(CFLAGS) -o $@

explore: explore.o buffer.o mapfile.o nodeutils.o
	$(CC) $^ $(CFLAGS) -o $@

route: route.o buffer.o mapfile.o nodeutils.o
	$(CC) $^ $(CFLAGS) -o $@

removeduplicates: removeduplicates.o buffer.o
	$(CC) $^ $(CFLAGS) -o $@

printlinks-test: printlinks-test.o buffer.o printlinks.o
	$(CC) $^ $(CFLAGS) -o $@

%.o: %.c %.h
	$(CC) -c $< $(CFLAGS) -o $@
%.o: %.c
	$(CC) -c $< $(CFLAGS) -o $@

links.txt: wikidump.bz2 extractlinks presort removeduplicates
	@echo Note: The progress bar only shows the progress of the first step
	pv wikidump.bz2 | bzcat | ./extractlinks - | ./presort | LC_ALL=C sort | cut -b 2- | ./removeduplicates >links.txt

nodes.bin: links.txt parselinks
	./parselinks links.txt nodes.bin titles.txt
	./verifynodes nodes.bin titles.txt

clean:
	rm -f $(ALL) *.o
