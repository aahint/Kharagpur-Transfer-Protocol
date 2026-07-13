all: initksocket user1 user2 

libksocket.a: ksocket.o
	ar rcs libksocket.a ksocket.o

ksocket.o: ksocket.c ksocket.h
	gcc -DDEBUG -c ksocket.c

initksocket: initksocket.c libksocket.a
	gcc -DDEBUG -o initksocket initksocket.c -L. -lksocket

user1: user1.c libksocket.a
	gcc -DDEBUG -o user1 user1.c -L. -lksocket

user2: user2.c libksocket.a
	gcc -DDEBUG -o user2 user2.c -L. -lksocket

clean:
	rm -f *.o *.a initksocket user1 user2 a.out