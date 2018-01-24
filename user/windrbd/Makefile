# This is a very simple makefile

all: windrbd

windrbd: windrbd.c
	gcc -g -Wall -o windrbd.exe windrbd.c

install: windrbd
	install windrbd.exe /usr/sbin

clean:
	rm windrbd.exe
