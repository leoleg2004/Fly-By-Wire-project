# Project_Kernel_Trace - Unified Build System

# Default core count for parallel build if not specified
JOBS ?= 4

.PHONY: all libs apps clean rebuild help

# Build everything
all: libs apps
	@echo "========================================================="
	@echo " Build complete! Binaries are in bin/app/"
	@echo "========================================================="

# Reconstruct libmsg.a from DDS components
libmsg:
	@mkdir -p bin/lib/tmp_ext
	@echo ">>> Reconstructing libmsg.a from DDS objects..."
	@cd bin/lib/tmp_ext && for f in ../../../src/msg/DDS/bin/messages/lib/*.a; do ar x $$f; done && ar rcs ../libmsg.a *.o
	@rm -rf bin/lib/tmp_ext

# Build libraries first (mandatory dependency for apps)
libs: libmsg
	@echo ">>> Building Core Libraries..."
	$(MAKE) -C src/lib

# Build applications (depends on libs)
apps: libs
	@echo ">>> Building Applications..."
	$(MAKE) -C src/app

# Clean all build artifacts
clean:
	@echo ">>> Cleaning everything..."
	$(MAKE) -C src/lib clean
	$(MAKE) -C src/app clean
	rm -f bin/lib/libmsg.a
	@echo "Done."

# Full rebuild
rebuild: clean all

# Help message
help:
	@echo "Usage:"
	@echo "  make          - Build everything (sequential libraries, then apps)"
	@echo "  make -j4      - Build everything in parallel where possible"
	@echo "  make clean    - Remove all binaries and object files"
	@echo "  make rebuild  - Clean and then build everything"
