CXX = g++
PROJECT_ROOT = $(abspath ../../../../)
BIN_DIR = $(PROJECT_ROOT)/bin/msg/lib
OBJ_DIR = output

CORE_INC = -I$(PROJECT_ROOT)/src/lib/lib/activity/include \
           -I$(PROJECT_ROOT)/src/lib/lib/math/include \
           -I$(PROJECT_ROOT)/src/lib/lib/matrix/include \
           -I$(PROJECT_ROOT)/src/lib/lib/print/include \
           -I$(PROJECT_ROOT)/src/lib/lib/time/include \
           -I$(PROJECT_ROOT)/src/lib/lib/trace/include

CXXFLAGS = -Wall -I. -I../include $(CORE_INC) -fPIC -O2 -I/usr/local/include
LDFLAGS_SO = -shared

MSG_BASENAMES = geometry_msgs nav_msgs rover_msgs sensor_msgs std_msgs tf2_msgs

TARGETS_A = $(addprefix $(BIN_DIR)/libdds_, $(addsuffix .a, $(MSG_BASENAMES)))
TARGETS_SO = $(addprefix $(BIN_DIR)/libdds_, $(addsuffix .so, $(MSG_BASENAMES)))

all: $(TARGETS_A) $(TARGETS_SO)

define BUILD_MSG_LIB
$(BIN_DIR)/libdds_$(1).a: $(OBJ_DIR)/$(1).o $(OBJ_DIR)/$(1)PubSubTypes.o
	@mkdir -p $(BIN_DIR)
	@echo ">>> Archiving libdds_$(1).a..."
	ar rcs $$@ $$^

$(BIN_DIR)/libdds_$(1).so: $(OBJ_DIR)/$(1).o $(OBJ_DIR)/$(1)PubSubTypes.o
	@mkdir -p $(BIN_DIR)
	@echo ">>> Linking libdds_$(1).so..."
	$(CXX) $(LDFLAGS_SO) -o $$@ $$^
endef

$(foreach msg,$(MSG_BASENAMES),$(eval $(call BUILD_MSG_LIB,$(msg))))

$(OBJ_DIR)/%.o: %.cxx
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGETS_A) $(TARGETS_SO)

.PHONY: all clean
