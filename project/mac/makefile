SHELL := /bin/bash
PROJECT := usbio

all:
	cc -O2 -I../../imports/mac \
	../../source/main.cpp \
	-o ${PROJECT} \
	../../imports/mac/build/libft4222.a \
	-lstdc++ -lpthread -lobjc -ltcl \
	-framework IOKit -framework CoreFoundation

.PHONY: clean
clean:
	@echo Info: Cleaning...
	@rm -rf $(PROJECT)
