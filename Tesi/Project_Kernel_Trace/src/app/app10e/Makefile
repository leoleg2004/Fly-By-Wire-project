CC = gcc
CFLAGS = -Wall -O2 -I../../../src/lib -I../../../src/msg
LDFLAGS = -L../../../bin/lib -lActivity_library -lpthread -lm -lrt
TARGET = ../../../bin/app/app10e

all: $(TARGET)

$(TARGET): app10e.c
	@mkdir -p ../../../bin/app
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
