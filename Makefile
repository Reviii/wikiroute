all: parseXML reader1 tree1

extractlinks: extractlinks.o printlinks.o
	gcc extractlinks.o printlinks.o -lxml2 -o extractlinks

reader1: reader1.c
	gcc reader1.c -lxml2 -o reader1

tree1: tree1.c
	gcc tree1.c -lxml2 -o tree1

%.o: %.c
	gcc -c $< -o $@

clean:
	rm parseXML reader1 tree1 *.o
