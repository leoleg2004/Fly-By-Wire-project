CXX = g++
PROJECT_ROOT = $(abspath ../../../)
BIN_DIR = $(PROJECT_ROOT)/bin/app
LIB_DIR = $(PROJECT_ROOT)/bin/lib

# Folders for headers
CORE_HDR = -I$(PROJECT_ROOT)/src/lib/lib/activity/include -I$(PROJECT_ROOT)/src/lib/lib/math/include -I$(PROJECT_ROOT)/src/lib/lib/matrix/include -I$(PROJECT_ROOT)/src/lib/lib/print/include -I$(PROJECT_ROOT)/src/lib/lib/time/include -I$(PROJECT_ROOT)/src/lib/lib/trace/include
MSG_HDR = $(PROJECT_ROOT)/bin/msg/include

CFLAGS = -Wall -O2 -I. $(CORE_HDR) -I$(MSG_HDR)
LDFLAGS = -Wl,-rpath,$(LIB_DIR) -L$(LIB_DIR) -L$(PROJECT_ROOT)/bin/msg/lib -lActivity_library -lpthread -lm -lrt

TARGET = $(BIN_DIR)/F16_Computer
LIBS_DEPS = $(LIB_DIR)/libActivity_library.a

all: $(TARGET)

$(TARGET): F16_Computer.cpp $(LIBS_DEPS)
	@mkdir -p $(BIN_DIR)
	@echo ">>> Building F16_Computer..."
	$(CXX) $(CFLAGS) $< -o $@ $(LDFLAGS)
	@echo "Done."

clean:
	rm -f $(TARGET)

.PHONY: all clean
