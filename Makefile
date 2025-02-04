CC = gcc
CFLAGS = -Wall

SRC = src/
INCLUDE = include/
BIN = bin/

URL= ftp://ftp.up.pt/pub/gnu/emacs/elisp-manual-21-2.8.tar.gz
#URL= ftp://ftp.bit.nl/pub/welcome.msg

.PHONY: clean all run

all: $(BIN)/main

$(BIN)/main: main.c $(SRC)/*.c
	$(CC) $(CFLAGS) -o $@ $^ -I$(INCLUDE)

run: $(BIN)/main
	./$(BIN)/main $(URL)

clean:
	rm -f $(BIN)/*
