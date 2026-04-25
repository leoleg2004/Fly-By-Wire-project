#----------------------------------------------------------------------------
#       IDL DEFINITION
#----------------------------------------------------------------------------
IDL-TYPE = $(DDS_MSG)


#----------------------------------------------------------------------------
#       STAR FILE SYSTEM DEFINITION
#----------------------------------------------------------------------------
#INC_TARGET_DIR = $(AURORA_HOME)/src/messages/include
INC_TARGET_DIR = $(SERL_HOME)/DDS/bin/messages/include
LIB_TARGET_DIR = $(SERL_HOME)/DDS/bin/messages/lib

CPPFLAGS = -c -Wall -fpic -m64 -O2 -std=c++0x
LDFLAGS= -m64

CPP= g++
LN= g++
AR=ar
SYSLIBS= -ldl -lm -lpthread -lrt
DEFINES= 

INCLUDES= -I. 


LIBS =  -lfastcdr -lfastrtps $(SYSLIBS)

DIRECTORIES= output.dir

all: $(DIRECTORIES) $(IDL-TYPE) install clean


$(IDL-TYPE)_TARGET= $(LIB_TARGET_DIR)/libdds_$(IDL-TYPE).so
$(IDL-TYPE)_TARGET_Z= $(LIB_TARGET_DIR)/libdds_$(IDL-TYPE).a

$(IDL-TYPE)_IDL_SRC = $(IDL-TYPE).cxx \
	$(IDL-TYPE)PubSubTypes.cxx

$(IDL-TYPE)_IDL_OBJS    = $($(IDL-TYPE)_IDL_SRC:%.cxx=output/%.o) $($(IDL-TYPE)_COMMON_SRC_CPPFILES:%.cpp=output/%.o)

$(IDL-TYPE)OBJS+= $($(IDL-TYPE)_IDL_OBJS)

$($(IDL-TYPE)_TARGET): $($(IDL-TYPE)_IDL_OBJS)
	$(LN) $(LDFLAGS) -shared -o $($(IDL-TYPE)_TARGET) $($(IDL-TYPE)_IDL_OBJS) $(LIBS) -L$(LIB_TARGET_DIR)  

$($(IDL-TYPE)_TARGET_Z): $($(IDL-TYPE)_IDL_OBJS)
	$(AR) -cru $($(IDL-TYPE)_TARGET_Z) $($(IDL-TYPE)_IDL_OBJS)

$(IDL-TYPE): $($(IDL-TYPE)_TARGET) $($(IDL-TYPE)_TARGET_Z)

output/%.o:%.cxx
	$(CPP) $(CPPFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

output/%.o:%.cpp
	$(CPP) $(CPPFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

.PHONY: $(IDL-TYPE)

clean:
	@rm -r output

install:
	@cp *.h $(INC_TARGET_DIR)

%.dir : 
	@echo "Checking directory $*"
	@if [ ! -d $* ]; then \
		echo "Making directory $*"; \
		mkdir -p $* ; \
	fi;
