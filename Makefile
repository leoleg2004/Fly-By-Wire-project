# Project_Kernel_Trace - Unified Build System

JOBS ?= 4

.PHONY: all msg libs apps clean rebuild help

# Build everything
all: msg libs apps
	@echo "========================================================="
	@echo " Build complete! Binaries are in bin/app/"
	@echo "========================================================="

# Generate sources from IDLs
msg:
	@echo ">>> Generating Message Sources from IDLs..."
	$(MAKE) -C Project_Kernel_Trace/src/msg

# Build libraries (depends on msg)
libs: msg
	@echo ">>> Building Core and DDS Libraries..."
	$(MAKE) -C Project_Kernel_Trace/src/lib

# Build applications (depends on libs)
apps: libs
	@echo ">>> Building Applications..."
	$(MAKE) -C Project_Kernel_Trace/src/app

# Clean all build artifacts
clean:
	@echo ">>> Cleaning everything..."
	$(MAKE) -C Project_Kernel_Trace/src/msg clean
	$(MAKE) -C Project_Kernel_Trace/src/lib clean
	$(MAKE) -C Project_Kernel_Trace/src/app clean
	@echo "Done."

# Full rebuild
rebuild: clean all

# Help message
help:
	@echo "Usage:"
	@echo "  make          - Build everything"
	@echo "  make clean    - Remove all binaries and object files"
	@echo "  make rebuild  - Clean and then build everything"
