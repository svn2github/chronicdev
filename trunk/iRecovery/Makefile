CC = gcc
CFLAGS_OSX = -lusb -framework CoreFoundation -framework IOKit
CFLAGS_LNX = -lusb

all:
		@echo 'make linux or make macosx'
	
macosx:
		$(CC) iRecovery.c -o iRecovery $(CFLAGS_OSX)

linux:
		$(CC) iRecovery.c -o iRecovery $(CFLAGS_LNX)