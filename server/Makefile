
app=aesdsocket

CC=$(CROSS_COMPILE)gcc
CFLAGS += -Wall -Werror -g

default: all

all: $(app).c
	$(CC)  $(app).c -o $(app) $(CFLAGS)

.PHONY: clean
clean:
	rm -f $(app)

.PHONY: run
run: all
	./$(app)
