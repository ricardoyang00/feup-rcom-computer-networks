CC = gcc
CFLAGS = -Wall

SRC = src/
INCLUDE = include/
BIN = bin/

#URL= "ftp://ftp.up.pt/pub/gnu/emacs/elisp-manual-21-2.8.tar.gz"
URL= "ftp://demo:password@test.rebex.net/readme.txt" 				#fail login 220
#URL= "ftp://anonymous:anonymous@ftp.bit.nl/speedtest/100mb.bin"
#URL= "ftp://netlab1.fe.up.pt/pub.txt" 								#unknown host
#URL= "ftp://rcom:rcom@netlab1.fe.up.pt/pipe.txt"					#unknown host
#URL= "ftp://rcom:rcom@netlab1.fe.up.pt/files/pic1.jpg"				#unknown host
#URL= "ftp://rcom:rcom@netlab1.fe.up.pt/files/pic2.png"				#unknown host
#URL= "ftp://rcom:rcom@netlab1.fe.up.pt/files/crab.mp4"				#unknown host
#URL= "ftp://rcom:rcom@netlab1.fe.up.pt/debian/ls-lR.gz"			#unknown host
#URL = "ftp://rcom:rcom@netlab1.fe.up.pt/pub/parrot/iso/testing/Parrot-architect-5.3_amd64.iso"	#unknown host
#URL = "ftp://ftp.up.pt/pub/kodi/timestamp.txt"						#fail request resource 150

.PHONY: clean all run

all: $(BIN)/main

$(BIN)/main: main.c $(SRC)/*.c
	$(CC) $(CFLAGS) -o $@ $^ -I$(INCLUDE)

run: $(BIN)/main
	./$(BIN)/main $(URL)

clean:
	rm -f $(BIN)/*
