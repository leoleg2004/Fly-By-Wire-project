# Makefile for Message Generation in Project_Kernel_Trace

IDL_FILES = $(wildcard *.idl)
DONE_FILES = $(IDL_FILES:.idl=.done)

# Generation directories
GEN_INC = ../lib/generated_msg/include
GEN_SRC = ../lib/generated_msg/src
BIN_INC_DIR = ../../bin/msg/include

FAST_DDS_GEN = /home/leonardo/TOOLS/eProsima/src/fastddsgen/scripts/fastddsgen
# Ensure Java 11 is used
export JAVA_HOME = /usr/lib/jvm/java-1.11.0-openjdk-amd64
export PATH := $(JAVA_HOME)/bin:$(PATH)

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
