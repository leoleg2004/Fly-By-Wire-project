CXX = g++
PROJECT_ROOT = $(abspath ../../../)
BIN_DIR = $(PROJECT_ROOT)/bin/app
LIB_DIR = $(PROJECT_ROOT)/bin/lib
MSG_LIB_DIR = $(PROJECT_ROOT)/bin/msg/lib

# Folders for headers
CORE_HDR = -I$(PROJECT_ROOT)/src/lib/lib/activity/include -I$(PROJECT_ROOT)/src/lib/lib/math/include -I$(PROJECT_ROOT)/src/lib/lib/matrix/include -I$(PROJECT_ROOT)/src/lib/lib/print/include -I$(PROJECT_ROOT)/src/lib/lib/time/include -I$(PROJECT_ROOT)/src/lib/lib/trace/include
DDS_HDR = -I$(PROJECT_ROOT)/src/lib/dds_lib/broadcastner/include -I$(PROJECT_ROOT)/src/lib/dds_lib/listener/include
MSG_HDR = $(PROJECT_ROOT)/src/lib/generated_msg/include
IMG_HDR = -I$(PROJECT_ROOT)/src/lib/image_lib/sender/include -I$(PROJECT_ROOT)/src/lib/image_lib/subscriber/include -I$(PROJECT_ROOT)/src/lib/image_lib/listener/include

CXXFLAGS = -Wall -O2 -std=c++11 -I.  $(CORE_HDR)  $(DDS_HDR) -I$(MSG_HDR)  $(IMG_HDR) -I/usr/local/include
MSG_LIBS = -ldds_geometry_msgs -ldds_nav_msgs -ldds_rover_msgs -ldds_sensor_msgs -ldds_std_msgs -ldds_tf2_msgs
LDFLAGS = -L$(LIB_DIR) -L$(MSG_LIB_DIR) -lActivity_library -ldds_communication $(MSG_LIBS) -lpthread -lrt -L/usr/local/lib -lfastrtps -lfastcdr

TARGET = $(BIN_DIR)/Listener
LIBS_DEPS = $(LIB_DIR)/libActivity_library.a $(MSG_LIB_DIR)/libdds_communication.a $(wildcard $(MSG_LIB_DIR)/libdds_*.a)

# Automatically find all local and lib headers
HDR_DEPS = $(wildcard *.h) $(wildcard *.hpp) \
           $(wildcard $(CORE_HDR)/*.h) \
           $(wildcard $(DDS_HDR)/*.hpp) \
           $(wildcard $(MSG_HDR)/*.h) \
           $(wildcard $(IMG_HDR)/*.hpp)

all: $(TARGET)

$(TARGET): Listener.cpp $(LIBS_DEPS) $(HDR_DEPS)
	@mkdir -p $(BIN_DIR)
	@echo ">>> Building Listener..."
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)
	@echo "Done."

clean:
	rm -f $(TARGET)

.PHONY: all clean
