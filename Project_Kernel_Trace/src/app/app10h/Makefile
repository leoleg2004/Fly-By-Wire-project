CC = gcc
PROJECT_ROOT = $(abspath ../../../)
BIN_DIR = $(PROJECT_ROOT)/bin/app
LIB_DIR = $(PROJECT_ROOT)/bin/lib

# Folders for headers
CORE_HDR = -I$(PROJECT_ROOT)/src/lib/lib/activity/include -I$(PROJECT_ROOT)/src/lib/lib/math/include -I$(PROJECT_ROOT)/src/lib/lib/matrix/include -I$(PROJECT_ROOT)/src/lib/lib/print/include -I$(PROJECT_ROOT)/src/lib/lib/time/include -I$(PROJECT_ROOT)/src/lib/lib/trace/include
MSG_HDR = $(PROJECT_ROOT)/src/lib/generated_msg/include

CFLAGS = -Wall -O2 -I.  $(CORE_HDR) -I$(MSG_HDR)
LDFLAGS = -L$(LIB_DIR) -L$(PROJECT_ROOT)/bin/msg/lib -lActivity_library -lpthread -lm -lrt

TARGET = $(BIN_DIR)/app10h
LIBS_DEPS = $(LIB_DIR)/libActivity_library.a $(PROJECT_ROOT)/bin/msg/lib/libdds_geometry_msgs.a

HDR_DEPS = $(wildcard *.h) $(wildcard $(CORE_HDR)/*.h) $(wildcard $(MSG_HDR)/*.h)

all: $(TARGET)

$(TARGET): app10h.c $(LIBS_DEPS) $(HDR_DEPS)
	@mkdir -p $(BIN_DIR)
	@echo ">>> Building app10h..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
	@echo "Done."

clean:
	rm -f $(TARGET)

.PHONY: all clean
