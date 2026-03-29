#CROSS_COMPILE = aarch64-none-linux-gnu-gcc
CC=gcc
TARGET = writer
SRC = writer.c

all: 
	$(CROSS_COMPILE)$(CC) $(SRC) -o $(TARGET)
#	gcc $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET).o $(TARGET)

.PHONY: all clean
