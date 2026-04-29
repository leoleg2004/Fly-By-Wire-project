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

DDS_INC = -I$(PROJECT_ROOT)/src/lib/dds_lib/broadcastner/include \
          -I$(PROJECT_ROOT)/src/lib/dds_lib/listener/include

IMG_INC = -I$(PROJECT_ROOT)/src/lib/image_lib/sender/include \
          -I$(PROJECT_ROOT)/src/lib/image_lib/subscriber/include \
          -I$(PROJECT_ROOT)/src/lib/image_lib/listener/include

MSG_INC = -I$(PROJECT_ROOT)/bin/msg/include

CXXFLAGS = -Wall -I. -I../include $(CORE_INC) $(DDS_INC) $(IMG_INC) $(MSG_INC) -fPIC -O2 -I/usr/local/include $(shell pkg-config --cflags opencv4)

SRC = $(wildcard *.cpp)
OBJS = $(addprefix $(OBJ_DIR)/, $(SRC:.cpp=.o))

LIB_NAME = libimage_sender.a
TARGET = $(BIN_DIR)/$(LIB_NAME)

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	@if [ -n "$(OBJS)" ]; then \
		echo ">>> Archiving $(LIB_NAME)..."; \
		ar rcs $@ $^; \
	else \
		echo ">>> No sources for $(LIB_NAME), skipping archive."; \
	fi

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean
