# Makefile per src/msg in Project_Kernel_Trace

CXX = g++
CXXFLAGS = -Wall -fPIC -O2 -std=c++11
GEN_DIR = generated
BIN_LIB_DIR = ../../bin/lib

all: $(BIN_LIB_DIR)/libmsg.a

$(BIN_LIB_DIR)/libmsg.a: 
	@mkdir -p $(BIN_LIB_DIR)
	@mkdir -p $(GEN_DIR)
	@echo "Compilazione sorgenti DDS in $(GEN_DIR)..."
	@for f in $$(ls $(GEN_DIR)/*.cxx 2>/dev/null); do \
		echo "  Compilazione $$f..."; \
		$(CXX) $(CXXFLAGS) -I. -I$(GEN_DIR) -I../lib -I/usr/local/include -c $$f -o $${f%.cxx}.o; \
	done
	@OBJ_FILES=$$(ls $(GEN_DIR)/*.o 2>/dev/null); \
	if [ -n "$$OBJ_FILES" ]; then \
		echo "  Creazione libmsg.a con $$(echo $$OBJ_FILES | wc -w) oggetti..."; \
		ar rcs $@ $$OBJ_FILES; \
	else \
		echo "  ATTENZIONE: Nessun oggetto trovato in $(GEN_DIR)!"; \
		touch $@; \
	fi

clean:
	rm -f $(GEN_DIR)/*.o
	rm -f $(BIN_LIB_DIR)/libmsg.a

.PHONY: all clean
