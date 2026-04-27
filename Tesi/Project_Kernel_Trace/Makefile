# Makefile Principale del Progetto Project_Kernel_Trace

# Directory principali
SRC_DIR = src
BIN_DIR = bin
# La cartella Test deve finire dentro bin
TEST_DIR = $(BIN_DIR)/Test

# Target di default
all: build_all

build_all:
	@mkdir -p $(BIN_DIR)/app $(BIN_DIR)/lib $(BIN_DIR)/msg $(BIN_DIR)/tools
	@mkdir -p $(TEST_DIR)
	-$(MAKE) -C $(SRC_DIR)/lib
	-$(MAKE) -C $(SRC_DIR)/msg
	-$(MAKE) -C $(SRC_DIR)/app

# Pulizia
clean:
	-$(MAKE) -C $(SRC_DIR)/lib clean
	-$(MAKE) -C $(SRC_DIR)/msg clean
	-$(MAKE) -C $(SRC_DIR)/app clean
	# Manteniamo (BIN_DIR)/tools/ perché contiene gli script "sorgenti" e thread_analysis.cpp
	-rm -rf $(BIN_DIR)/app $(BIN_DIR)/lib $(BIN_DIR)/msg
	-rm -rf $(BIN_DIR)/Test/* $(BIN_DIR)/test/* 2>/dev/null

# Re-compilazione completa
rebuild: clean all

.PHONY: all clean rebuild build_all
