CROSS_COMPILE = aarch64-none-linux-gnu-gcc
TARGET = writer
SRC = writer.c

all: $(TARGET)
$(TARGET): $(SRC)
# 	$(CROSS_COMPILE) $(SRC) -o $(TARGET)
	gcc $(SRC) -o $(TARGET)

clean:
	rm -f /home/zhya/work/finder-app/$(TARGET)

.PHONY: all clean