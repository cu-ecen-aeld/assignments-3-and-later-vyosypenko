
ifndef CROSS_COMPILE
CC=gcc
else
CC=$(CROSS_COMPILE)gcc
endif


default: program

program: writer.c
	$(CC)  writer.c -o writer

.PHONY: clean
clean:
	rm -f writer
