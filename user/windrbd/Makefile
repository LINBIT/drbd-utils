# This is a very simple makefile

all: windrbd

windrbd: windrbd.c
	gcc -o windrbd.exe windrbd.c

install: windrbd
	install windrbd.exe /usr/sbin
