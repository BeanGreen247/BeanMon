CC=gcc
CFLAGS=-Wall -lX11
TARGET=BeanMon

all:
	$(CC) -o $(TARGET) main.c $(CFLAGS)

clean:
	rm -f $(TARGET)
