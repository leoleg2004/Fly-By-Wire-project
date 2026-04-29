CC = gcc
OBJ_DIR = output
PROJECT_ROOT = $(abspath ../../../../../)
BIN_DIR = $(PROJECT_ROOT)/bin/lib

CFLAGS = -Wall -fPIC -O2 \
         -I../include \
         -I../../activity/include \
         -I../../math/include \
         -I../../matrix/include \
         -I../../print/include \
         -I../../time/include \
         -I../../trace/include

SRC = $(wildcard *.c)
OBJS = $(addprefix $(OBJ_DIR)/, $(SRC:.c=.o))

LIB_NAME = libtrace.a
TARGET = $(BIN_DIR)/$(LIB_NAME)
COMBINED_LIB = $(BIN_DIR)/libActivity_library.a

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	@echo ">>> Archiving $(LIB_NAME)..."
	ar rcs $@ $^
	ar rcs $(COMBINED_LIB) $^

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean
