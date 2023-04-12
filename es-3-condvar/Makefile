TARGET = es3
LIBS = -lm
CC = gcc
CFLAGS = -g -Wall

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c)) $(patsubst lib/%.c, lib/%.o, $(wildcard lib/*.c))
HEADERS = $(wildcard *.h)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) -pthread -g $(OBJECTS) -Wall $(LIBS) -o $@

clean:
	-rm -f lib/*.o
	-rm -f *.o
	-rm -f $(TARGET)