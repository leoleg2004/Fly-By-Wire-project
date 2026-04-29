CXX = g++
OBJ_DIR = output
PROJECT_ROOT = $(abspath ../../../../../)
BIN_DIR = $(PROJECT_ROOT)/bin/msg/lib

CORE_INC = -I$(PROJECT_ROOT)/src/lib/lib/activity/include \
           -I$(PROJECT_ROOT)/src/lib/lib/math/include \
           -I$(PROJECT_ROOT)/src/lib/lib/matrix/include \
           -I$(PROJECT_ROOT)/src/lib/lib/print/include \
           -I$(PROJECT_ROOT)/src/lib/lib/time/include \
           -I$(PROJECT_ROOT)/src/lib/lib/trace/include

MSG_INC = -I$(PROJECT_ROOT)/bin/msg/include

CXXFLAGS = -Wall -I. -I../include $(CORE_INC) $(MSG_INC) -fPIC -O2 -I/usr/local/include

SRC = $(wildcard *.cpp)
OBJS = $(addprefix $(OBJ_DIR)/, $(SRC:.cpp=.o))

LIB_NAME = liblistener.a
TARGET = $(BIN_DIR)/$(LIB_NAME)
COMBINED_LIB = $(BIN_DIR)/libdds_communication.a

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	@echo ">>> Archiving $(LIB_NAME)..."
	ar rcs $@ $^
	ar rcs $(COMBINED_LIB) $^

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean
