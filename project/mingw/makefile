SHELL := /bin/bash

all: clean compile link
	cp ../../imports/windows/LibFT4222/dll/amd64/LibFT4222-64.dll .
	rm main.o

compile:
	gcc -c -g -O2 \
		-I../../source \
		-I../../imports/windows/ftd2xx \
		-I../../imports/windows/LibFT4222/inc \
		-I/mingw64/include \
		-DWINDOWS \
		-D_WIN32 \
		-o main.o \
		../../source/main.cpp

link:
	gcc -o usbio.exe main.o \
		-L../../imports/windows/ftd2xx/dll/amd64 \
		-L../../imports/windows/LibFT4222/dll/amd64 \
		-L/mingw64/lib \
		-lstdc++ \
		-lftd2xx \
		-lLibFT4222-64 \
		-ltcl

.PHONY: clean
clean:
	rm -rf main.o usbio.exe
