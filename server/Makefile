CROSS_COMPILE ?=
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall -Werror

TARGET = aesdsocket
SRC = aesdsocket.c

all: $(TARGET)

default: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET) *.o
.PHONY: all default clean