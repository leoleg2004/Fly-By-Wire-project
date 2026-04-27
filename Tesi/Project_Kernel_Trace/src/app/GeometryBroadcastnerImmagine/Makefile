CXX = g++
CXXFLAGS = -Wall -O2 -std=c++11 -I../../../src/lib -I../../../src/msg -I../../../src/msg/DDS -I/usr/local/include
LDFLAGS = -L../../../bin/lib -lActivity_library -lcommunication_library -lpthread -lrt -L/usr/local/lib -lfastrtps -lfastcdr
TARGET = ../../../bin/app/GeometryBroadcastnerImmagine

all: $(TARGET)

$(TARGET): GeometryBroadcastnerImmagine.cpp
	@mkdir -p ../../../bin/app
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
