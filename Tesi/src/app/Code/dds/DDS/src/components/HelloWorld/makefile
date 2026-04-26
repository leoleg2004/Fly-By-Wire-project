#----------------------------------------------------------------------------
#       COMPONENT DEFINITION
#----------------------------------------------------------------------------
COMPONENT       = HelloWorldPublisher

COMPONENT-SRC   = HelloWorldPublisher.cpp

PACKAGE-DIR   	= $(SERL_HOME)/PROJECT/bin/components
COMPONENT_TARGET= $(PACKAGE-DIR)/$(COMPONENT)

 
#----------------------------------------------------------------------------
#       REQUIRED IDL LIBRARIES
#----------------------------------------------------------------------------
IDL-INCLUDE_dir   = -I$(SERL_HOME)/PROJECT/bin/messages/include
IDL-LIB_dir       = -L$(SERL_HOME)/PROJECT/bin/messages/lib
IDL-LIBs          = -ldds_nav_msgs -ldds_sensor_msgs -ldds_geometry_msgs -ldds_std_msgs

#----------------------------------------------------------------------------
#       REQUIRED RUNTIME LIBRARIES
#----------------------------------------------------------------------------
RUNTIME= $(SERL_HOME)/bin/runtime

RUNTIME_LIB  = -L$(SERL_HOME)/PROJECT/bin/runtime/lib/ -ltime -lactivity -lcommunication
RUNTIME_INC  = -I$(SERL_HOME)/PROJECT/bin/runtime/include



#OPENCV
OPENCV-INCLUDE_dir   = -I/usr/include/opencv4/
OPENCV-LIBs          = `pkg-config opencv4 --cflags --libs`



#----------------------------------------------------------------------------
#       REQUIRED SYSTEM LIBRARIES
#-------------------------------------rpath=/home/user/.usr/lib----------------------------------------
# E-PROSIMA DDS
DDS-LIBs          = -lfastcdr -lfastrtps

#SYSTEM
SYS-LIBs	  = -ldl -lm -ludev #-lnsl

#BOOST
BOOST-LIBs	  = -lpthread -lrt -lboost_system -lboost_thread -lboost_filesystem


#----------------------------------------------------------------------------
#       COMPILER DIRECTIVES
#----------------------------------------------------------------------------
CPP= g++
CPPFLAGS = -c -Wall -fpic -m64 -O2 -std=c++0x
LN= g++
AR=ar
LDFLAGS= -m64
#SYSLIBS= -ldl -lnsl -lm -lpthread -lrt -lboost_system -lboost_thread
DEFINES= 

INCLUDES= -Iinclude/ $(IDL-INCLUDE_dir) $(RUNTIME_INC) $(OPENCV-INCLUDE_dir)



Static_LIBS  = $(IDL-LIB_dir)/ $(IDL-LIBs) $(RUNTIME_LIB)
Dynamic_LIBS = $(RUNTIME_LIB) $(DDS-LIBs) $(BOOST-LIBs) $(SYS-LIBs) $(OPENCV-LIBs) 


DIRECTORIES= output.dir


all: $(DIRECTORIES) $(COMPONENT_TARGET) clean 

#COMPONENT_OBJS = $(COMPONENT-SRC:src/%.cpp=output/%.o)
COMPONENT_OBJS = $(COMPONENT-SRC:%.cpp=output/%.o)

OBJS+= $(COMPONENT_OBJS)

$(COMPONENT_TARGET): $(COMPONENT_OBJS)  
	$(LN) $(LDFLAGS) -o $@ $(COMPONENT_OBJS) -Wl,-Bstatic $(Static_LIBS) -Wl,-Bdynamic $(Dynamic_LIBS)

output/%.o:%.cxx
	$(CPP) $(CPPFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

output/%.o:%.cpp
	$(CPP) $(CPPFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

.PHONY: $(COMPONENT_TARGET) 

clean:
	@rm -r output

install:
	@cp ../cfg/$(COMPONENT).cfg $(CFG-DIR)

%.dir : 
	@echo "Checking directory $*"
	@if [ ! -d $* ]; then \
		echo "Making directory $*"; \
		mkdir -p $* ; \
	fi;
