all: mypipeline myshell

mypipeline: mypipeline.o
	gcc -Wall -g -O0 -o mypipeline mypipeline.o

mypipeline.o: mypipeline.c
	gcc -Wall -g -O0 -c mypipeline.c

myshell: myshell.o LineParser.o
	gcc -Wall -g -O0 -o myshell myshell.o LineParser.o

myshell.o: myshell.c lab_2_copy/LineParser.h
	gcc -Wall -g -O0 -c myshell.c -I./lab_2_copy

LineParser.o: lab_2_copy/LineParser.c lab_2_copy/LineParser.h
	gcc -Wall -g -O0 -c lab_2_copy/LineParser.c

.PHONY: all clean

clean:
	rm -f *.o mypipeline myshell