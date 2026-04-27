CXX = g++
PROJECT_ROOT = $(abspath ../../../)
CXXFLAGS = -Wall -O2 -std=c++11 -I. -I$(PROJECT_ROOT)/src/lib -I$(PROJECT_ROOT)/src/lib/communication_library -I$(PROJECT_ROOT)/src/msg -I$(PROJECT_ROOT)/src/msg/generated -I/usr/local/include
LDFLAGS = -L$(PROJECT_ROOT)/bin/lib -lActivity_library -lcommunication_library -lmsg -lpthread -lrt -L/usr/local/lib -lfastrtps -lfastcdr
TARGET = $(PROJECT_ROOT)/bin/app/Broadcastner

all: $(TARGET)

$(TARGET): Broadcastner.cpp
	@mkdir -p $(PROJECT_ROOT)/bin/app
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
