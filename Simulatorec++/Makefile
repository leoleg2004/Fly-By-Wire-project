# =============================================================================
# WRAPPER MAKEFILE (delegates to CMake)
# =============================================================================

BUILD_DIR = build
CMAKE = cmake
MAKE_OPTS = --no-print-directory

.PHONY: all clean regenerate_idl FlightSim MonitorApp help

# Default target
all: $(BUILD_DIR)/Makefile
	@echo "--- Building project (Parallel) ---"
	@$(MAKE) $(MAKE_OPTS) -C $(BUILD_DIR)

# Configure CMake if it hasn't been done
$(BUILD_DIR)/Makefile:
	@echo "--- Initializing CMake configuration ---"
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) -DCMAKE_BUILD_TYPE=Release ..

# Specific targets
FlightSim: $(BUILD_DIR)/Makefile
	@echo "--- Building FlightSim ---"
	@$(MAKE) $(MAKE_OPTS) -C $(BUILD_DIR) FlightSim

MonitorApp: $(BUILD_DIR)/Makefile
	@echo "--- Building MonitorApp ---"
	@$(MAKE) $(MAKE_OPTS) -C $(BUILD_DIR) MonitorApp

regenerate_idl: $(BUILD_DIR)/Makefile
	@echo "--- Regenerating DDS Type Stubs ---"
	@$(MAKE) $(MAKE_OPTS) -C $(BUILD_DIR) regenerate_idl

# Utility targets
clean:
	@echo "--- Cleaning build directory ---"
	@rm -rf $(BUILD_DIR)

help:
	@echo "Available targets:"
	@echo "  make            : Build everything"
	@echo "  make -j4        : Build everything in parallel (recommended)"
	@echo "  make FlightSim  : Build only the simulator"
	@echo "  make MonitorApp : Build only the monitor"
	@echo "  make clean      : Remove the build directory"
	@echo "  make help       : Show this message"
