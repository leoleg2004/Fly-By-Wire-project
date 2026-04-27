# Makefile per src/lib in Project_Kernel_Trace

CC = gcc
CXX = g++
CFLAGS = -Wall -I. -fPIC -O2
CXXFLAGS = -Wall -I. -fPIC -O2 
BIN_DIR = ../../bin/lib

# Tutti i .c e .cpp nella root di lib/ (tranne sottocartelle specifiche)
SRC_C = $(wildcard *.c)
SRC_CPP = $(wildcard *.cpp)
OBJS = $(SRC_C:.c=.o) $(SRC_CPP:.cpp=.o)

# Oggetti per communication_library
COM_SRC_C = $(wildcard communication_library/*.c)
COM_SRC_CPP = $(wildcard communication_library/*.cpp)
COM_OBJS = $(COM_SRC_C:.c=.o) $(COM_SRC_CPP:.cpp=.o)

all: $(BIN_DIR)/libActivity_library.a $(BIN_DIR)/libcommunication_library.a

$(BIN_DIR)/libActivity_library.a: $(OBJS)
	@mkdir -p $(BIN_DIR)
	ar rcs $@ $^

$(BIN_DIR)/libcommunication_library.a: $(COM_OBJS)
	@mkdir -p $(BIN_DIR)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(BIN_DIR)/lib*.a
	rm -f *.o communication_library/*.o

.PHONY: all clean
