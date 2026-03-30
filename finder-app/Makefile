CROSS_COMPILE ?=
CC = $(CROSS_COMPILE)gcc

TARGET = writer
SRC = writer.c

all: 
# 	$(CROSS_COMPILE)$(CC) $(SRC) -o $(TARGET)
	$(CC) $(SRC) -o $(TARGET)
clean:
	rm -f $(TARGET).o $(TARGET)
.PHONY: all clean
