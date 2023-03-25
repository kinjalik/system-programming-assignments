all: test.o userfs.o
	gcc test.o userfs.o

test.o: test.c
	gcc -g -c test.c -o test.o -I ../utils -Wall

userfs.o: userfs.c
	gcc -g -c userfs.c -o userfs.o -Wall

clean :
	-rm test.o userfs.o a.out