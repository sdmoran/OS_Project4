LIB=-lpthread
CC=gcc
CCPP=g++

proj4:proj4.c
	$(CC) proj4.c -o proj4 $(LIB)
