PROJECT_ROOT = $(abspath ../../../../../)
BIN_INC_DIR = $(PROJECT_ROOT)/bin/msg/include

all:
	@mkdir -p $(BIN_INC_DIR)
	@echo ">>> Copying headers for print..."
	@cp *.h $(BIN_INC_DIR)/ 2>/dev/null || true

clean:
	@echo "Cleaning headers for print..."

.PHONY: all clean
