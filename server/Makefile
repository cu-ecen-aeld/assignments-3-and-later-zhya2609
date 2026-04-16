CROSS_COMPILE ?=
CC = $(CROSS_COMPILE)gcc

TARGET = aesdsocket
SRC = aesdsocket.c

all: 
# 	$(CROSS_COMPILE)$(CC) $(SRC) -o $(TARGET)
	$(CC) $(SRC) -o $(TARGET) -Wall -Werror
clean:
	rm -f $(TARGET).o $(TARGET)
.PHONY: all clean