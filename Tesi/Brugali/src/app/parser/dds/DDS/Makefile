# =============================================================================
#  Makefile — dds/DDS/
#  Compila runtime libs + componenti SendImage e lancia il trace dedicato.
#
#  Target principali:
#    make all         → compila runtime + componenti
#    make runtime     → solo libtime, libactivity, libcommunication
#    make components  → solo ImageSender e ImageSubscriber (richiede runtime)
#    make clean       → elimina oggetti e librerie compilate
#    make trace_ImageSender     → compila + lancia run_trace_dds.sh su ImageSender
#    make trace_ImageSubscriber → compila + lancia run_trace_dds.sh su ImageSubscriber
# =============================================================================

# Carica SERL_HOME (necessario per tutti i sub-make)
# Viene fatto tramite shell in ogni ricetta che ne ha bisogno.
SHELL := /bin/bash
ENV   := source ./environment.sh &&

RUNTIME_DIR    = src/runtime
COMPONENT_DIR  = src/components/SendImage
BIN_RUNTIME    = bin/runtime/lib
BIN_COMPONENTS = bin/components
BIN_MSG_INC    = bin/messages/include
BIN_MSG_LIB    = bin/messages/lib

.PHONY: all runtime components clean \
        trace_ImageSender trace_ImageSubscriber \
        marker_ImageSender marker_ImageSubscriber marker_both \
        run_sender run_subscriber print

# ---------------------------------------------------------------------------
all: runtime components print

# ---------------------------------------------------------------------------
#  RUNTIME LIBRARIES  (time → activity → communication)
# ---------------------------------------------------------------------------
runtime: $(BIN_RUNTIME)
	@echo "[DDS] Compilazione runtime libraries..."
	$(ENV) $(MAKE) -C $(RUNTIME_DIR) -f time_makefile
	$(ENV) $(MAKE) -C $(RUNTIME_DIR) -f activity_makefile
	$(ENV) $(MAKE) -C $(RUNTIME_DIR) -f communication_makefile
	@echo "[DDS] Runtime pronto."

$(BIN_RUNTIME):
	@mkdir -p $@

# ---------------------------------------------------------------------------
#  COMPONENTI  (ImageSender, ImageSubscriber)
# ---------------------------------------------------------------------------
components: runtime $(BIN_COMPONENTS)
	@echo "[DDS] Compilazione componenti SendImage..."
	$(ENV) $(MAKE) -C $(COMPONENT_DIR) -f sender_makefile
	$(ENV) $(MAKE) -C $(COMPONENT_DIR) -f subscriber_makefile
	@echo "[DDS] Componenti pronti in $(BIN_COMPONENTS)/."

$(BIN_COMPONENTS):
	@mkdir -p $@

# ---------------------------------------------------------------------------
#  CLEAN
# ---------------------------------------------------------------------------
clean:
	@echo "[CLEAN] Pulizia librerie compilate..."
	@rm -f $(BIN_RUNTIME)/libtime.so    $(BIN_RUNTIME)/libtime.a
	@rm -f $(BIN_RUNTIME)/libactivity.so  $(BIN_RUNTIME)/libactivity.a
	@rm -f $(BIN_RUNTIME)/libcommunication.so $(BIN_RUNTIME)/libcommunication.a
	@rm -f $(BIN_COMPONENTS)/ImageSender $(BIN_COMPONENTS)/ImageSubscriber
	@echo "[CLEAN] Pulizia completata."

# ---------------------------------------------------------------------------
#  TRACE  (compila se necessario, poi lancia run_trace_dds.sh)
# ---------------------------------------------------------------------------
trace_ImageSender: components
	sudo bash run_trace_dds.sh ImageSender

trace_ImageSubscriber: components
	sudo bash run_trace_dds.sh ImageSubscriber

# ---------------------------------------------------------------------------
#  MARKER  (processi già avviati, Ctrl+C per fermare)
# ---------------------------------------------------------------------------
marker_ImageSender:
	sudo bash marker_dds.sh ImageSender

marker_ImageSubscriber:
	sudo bash marker_dds.sh ImageSubscriber

marker_both:
	sudo bash marker_dds.sh ImageSender ImageSubscriber

# ---------------------------------------------------------------------------
#  RUN  (senza trace, ma con LD_LIBRARY_PATH corretto)
# ---------------------------------------------------------------------------
run_sender: components
	$(ENV) $(BIN_COMPONENTS)/ImageSender

run_subscriber: components
	$(ENV) $(BIN_COMPONENTS)/ImageSubscriber

# ---------------------------------------------------------------------------
print:
	@echo "========================================================="
	@echo " Build DDS completata."
	@echo " Eseguibili in: $(BIN_COMPONENTS)/"
	@ls $(BIN_COMPONENTS)/ 2>/dev/null | grep -v '\.sh' || echo "  (nessuno)"
	@echo ""
	@echo " Per tracciare (lancia l'eseguibile + trace):"
	@echo "   make trace_ImageSender"
	@echo "   make trace_ImageSubscriber"
	@echo ""
	@echo " Per tracciare processi GIA' avviati (marker, Ctrl+C per fermare):"
	@echo "   make marker_ImageSender"
	@echo "   make marker_ImageSubscriber"
	@echo "   make marker_both          (sender + subscriber insieme)"
	@echo ""
	@echo " Per eseguire senza trace:"
	@echo "   make run_sender"
	@echo "   make run_subscriber"
	@echo "========================================================="
