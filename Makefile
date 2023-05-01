CFLAGS=-g -Wall -Werror --std=c99

mfs: mfs.c
	gcc -o mfs ${CFLAGS} mfs.c