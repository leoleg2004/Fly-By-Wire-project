CXX = g++
OBJ_DIR = output
PROJECT_ROOT = $(abspath ../../../../../../)
BIN_DIR = $(PROJECT_ROOT)/bin/lib

CFLAGS = -Wall -fPIC -O2 \
         -I../include \
         -I../../../activity/include \
         -I../../../math/include \
         -I../../../matrix/include \
         -I../../../print/include \
         -I../../../time/include \
         -I../../../trace/include

SRC = $(wildcard *.cpp)
OBJS = $(addprefix $(OBJ_DIR)/, $(SRC:.cpp=.o))

LIB_NAME = libf16_nav_gps.a
TARGET = $(BIN_DIR)/$(LIB_NAME)
COMBINED_LIB = $(BIN_DIR)/libActivity_library.a

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	@if [ -n "$(OBJS)" ]; then \
		echo ">>> Archiving $(LIB_NAME)..."; \
		ar rcs $@ $^; \
		ar rcs $(COMBINED_LIB) $^; \
	else \
		echo ">>> No sources to compile for $(LIB_NAME)"; \
	fi

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean
