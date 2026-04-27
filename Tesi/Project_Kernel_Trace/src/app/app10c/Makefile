CC = gcc
CFLAGS = -Wall -O2 -I../../../src/lib -I../../../src/msg
LDFLAGS = -L../../../bin/lib -lActivity_library -lpthread -lm -lrt
TARGET = ../../../bin/app/app10c

all: $(TARGET)

$(TARGET): app10c.c
	@mkdir -p ../../../bin/app
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
