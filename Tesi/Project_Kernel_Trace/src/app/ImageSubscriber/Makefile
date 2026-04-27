CXX = g++
PROJECT_ROOT = $(abspath ../../../)
# Aggiunto supporto OpenCV
OPENCV_FLAGS = $(shell pkg-config --cflags --libs opencv4)

CXXFLAGS = -Wall -O2 -std=c++11 \
           -I. \
           -I$(PROJECT_ROOT)/src/lib \
           -I$(PROJECT_ROOT)/src/lib/communication_library \
           -I$(PROJECT_ROOT)/src/msg \
           -I$(PROJECT_ROOT)/src/msg/generated \
           -I/usr/local/include \
           $(shell pkg-config --cflags opencv4)

LDFLAGS = -L$(PROJECT_ROOT)/bin/lib \
          -lmsg -lActivity_library -lcommunication_library \
          -lpthread -lrt \
          -L/usr/local/lib -lfastrtps -lfastcdr \
          $(shell pkg-config --libs opencv4)

TARGET = $(PROJECT_ROOT)/bin/app/ImageSubscriber

all: $(TARGET)

$(TARGET): ImageSubscriber.cpp
	@mkdir -p $(PROJECT_ROOT)/bin/app
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
