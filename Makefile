all: mypipeline myshell

mypipeline: mypipeline.o
	gcc -Wall -g -O0 -o mypipeline mypipeline.o

mypipeline.o: mypipeline.c
	gcc -Wall -g -O0 -c mypipeline.c

myshell: myshell.o LineParser.o
	gcc -Wall -g -O0 -o myshell myshell.o LineParser.o

myshell.o: myshell.c LineParser.h
	gcc -Wall -g -O0 -c myshell.c

LineParser.o: LineParser.c LineParser.h
	gcc -Wall -g -O0 -c LineParser.c

.PHONY: all clean

clean:
	rm -f *.o mypipeline myshell