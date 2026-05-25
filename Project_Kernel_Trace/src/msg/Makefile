# Makefile for Message Generation in Project_Kernel_Trace

IDL_FILES = $(wildcard *.idl)
DONE_FILES = $(IDL_FILES:.idl=.done)

# Generation directories
GEN_INC = ../lib/generated_msg/include
GEN_SRC = ../lib/generated_msg/src
BIN_INC_DIR = ../../bin/msg/include

# Locate fastddsgen: check PATH first, then common installation directories
FAST_DDS_GEN := $(shell \
	command -v fastddsgen 2>/dev/null || \
	( [ -f "/opt/fastdds/bin/fastddsgen" ] && echo "/opt/fastdds/bin/fastddsgen" ) || \
	( [ -f "$$HOME/eprosima_install/bin/fastddsgen" ] && echo "$$HOME/eprosima_install/bin/fastddsgen" ) || \
	( [ -f "$$HOME/TOOLS/eProsima/fastddsgen/scripts/fastddsgen" ] && echo "$$HOME/TOOLS/eProsima/fastddsgen/scripts/fastddsgen" ) || \
	( [ -f "$$HOME/TOOLS/eProsima/src/fastddsgen/scripts/fastddsgen" ] && echo "$$HOME/TOOLS/eProsima/src/fastddsgen/scripts/fastddsgen" ) || \
	echo "fastddsgen" \
)

# Try to find a valid JAVA_HOME if not already defined (required by fastddsgen)
ifndef JAVA_HOME
  JAVA_CANDIDATE := $(shell \
	for dir in /usr/lib/jvm/java-11-openjdk-amd64 /usr/lib/jvm/java-8-openjdk-amd64 /usr/lib/jvm/java-17-openjdk-amd64 /usr/lib/jvm/default-java; do \
		if [ -d "$$dir" ]; then echo "$$dir"; break; fi; \
	done \
  )
  ifneq ($(JAVA_CANDIDATE),)
    export JAVA_HOME := $(JAVA_CANDIDATE)
    export PATH := $(JAVA_HOME)/bin:$(PATH)
  endif
endif

all: $(DONE_FILES)

%.done: %.idl
	@echo "Generating sources for $<..."
	@mkdir -p temp_gen_$*
	@mkdir -p $(GEN_INC)
	@mkdir -p $(GEN_SRC)
	@mkdir -p $(BIN_INC_DIR)
	$(FAST_DDS_GEN) -I . -d temp_gen_$* -replace $<
	@echo "Moving files for $* to final destinations..."
	@mv temp_gen_$*/*.h temp_gen_$*/*.hpp temp_gen_$*/*.ipp $(GEN_INC)/ 2>/dev/null || true
	@mv temp_gen_$*/*.cxx $(GEN_SRC)/ 2>/dev/null || true
	@cp $(GEN_INC)/$*.h $(BIN_INC_DIR)/ 2>/dev/null || true
	@rm -rf temp_gen_$*
	touch $@

clean:
	rm -f *.done
	rm -rf temp_gen_*
	rm -f $(GEN_SRC)/*.cxx
	rm -f $(GEN_INC)/*.h $(GEN_INC)/*.hpp $(GEN_INC)/*.ipp
	rm -rf $(BIN_INC_DIR)

.PHONY: all clean
