PROJECT_ROOT = $(abspath ../../../../../../)
BIN_INC_DIR = $(PROJECT_ROOT)/bin/msg/include

all:
	@mkdir -p $(BIN_INC_DIR)
	@echo ">>> Copying headers for displays..."
	@cp *.hpp $(BIN_INC_DIR)/ 2>/dev/null || true

clean:
	@echo "Cleaning headers for displays..."

.PHONY: all clean
