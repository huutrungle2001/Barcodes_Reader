CC=gcc
CFLAGS=-g -Wall -std=c99
LIBS= 
TARGET=barcode_reader

DEPS = bitmap.h
OBJS = main.o bitmap.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIBS)

.PHONY: clean
clean:
	$(RM) $(TARGET) $(OBJS)
