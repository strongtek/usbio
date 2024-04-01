SHELL := /bin/bash

all: clean compile link
	rm main.o

compile:
	gcc -c -g -O2 -static \
		-I../../source \
		-I/usr/local/include \
		-I/usr/include \
		-I/usr/include/tcl \
		-DLINUX \
		-o main.o \
		../../source/main.cpp

link:
	gcc -o usbio -Wl,-rpath,/usr/local/lib \
		./main.o -L/usr/local/lib \
		-L/usr/lib \
		-lstdc++ \
		-lft4222 \
		-ltcl \
		-ldl \
		-lpthread \
		-lrt

.PHONY: clean
clean:
	rm -rf main.o usbio
