CROSS_COMPILE ?=
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall -Werror
USE_AESD_CHAR_DEVICE ?= 1

ifeq ($(USE_AESD_CHAR_DEVICE),1)
CFLAGS += -DUSE_AESD_CHAR_DEVICE
endif

TARGET = aesdsocket
SRC = aesdsocket.c

all: $(TARGET)

default: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) -o $(TARGET) -lpthread -lrt

clean:
	rm -f $(TARGET) *.o
.PHONY: all default clean