# e tutte le applicazioni DDS (GeometryPeriodic, Forwarder, ForwarderReply, ThreadAnalysisDDS)
#
# Comando per compilare tutto in parallelo:
#   make -j4
#
# Post-build: tutti gli eseguibili vengono copiati in parser/eseguibili/
#             tutti i sorgenti vengono copiati in parser/sorgenti/
# Questo è necessario affinché run_trace possa fare il parsing corretto.

CC = gcc
CXX = g++
# -MMD -MP per tracciare automaticamente le dipendenze degli HEADER
CFLAGS = -g -Wall -pthread -I.. -MMD -MP
LDFLAGS = -lm

# Librerie locali
LOCAL_LIBS = ../../lib/activity_library_dyn.a ../../lib/activity_library.a ../../lib/time_library.a
LIBS = $(LOCAL_LIBS)

.DEFAULT_GOAL := all

# ===========================================================================
# DDS: Librerie e Applicazioni
# ===========================================================================
DDS_LIB_DIR = dds/lib

# Elenco di TUTTE le app DDS con i relativi makefile
DDS_APP_GeometryBroadcastner_DIR  = dds/app/GeometryPeriodic
DDS_APP_GeometryBroadcastner_MK   = GeometryBroadcastner_makefile
DDS_APP_GeometryBroadcastner_SRCS = GeometryBroadcastner.cpp GeometryBroadcastner.hpp

# ---> AGGIUNTO: GeometryBroadcastnerImmagine <---
DDS_APP_GeometryBroadcastnerImmagine_DIR  = dds/app/GeometryPeriodic
DDS_APP_GeometryBroadcastnerImmagine_MK   = GeometryBroadcastnerImmagine_makefile
DDS_APP_GeometryBroadcastnerImmagine_SRCS = GeometryBroadcastnerImmagine.cpp GeometryBroadcastnerImmagine.hpp

DDS_APP_GeometryListener_DIR  = dds/app/GeometryPeriodic
DDS_APP_GeometryListener_MK   = GeometryListener_makefile
DDS_APP_GeometryListener_SRCS = GeometryListener.cpp GeometryListener.hpp

DDS_APP_Forwarder_Client_DIR  = dds/app/Forwarder
DDS_APP_Forwarder_Client_MK   = Client_makefile
DDS_APP_Forwarder_Client_EXE  = Client
DDS_APP_Forwarder_Client_SRCS = Client.cpp Client.hpp

DDS_APP_Forwarder_DivisionServer_DIR  = dds/app/Forwarder
DDS_APP_Forwarder_DivisionServer_MK   = DivisionServer_makefile
DDS_APP_Forwarder_DivisionServer_EXE  = DivisionServer
DDS_APP_Forwarder_DivisionServer_SRCS = DivisionServer.cpp DivisionServer.hpp

DDS_APP_Forwarder_Forwarder_DIR  = dds/app/Forwarder
DDS_APP_Forwarder_Forwarder_MK   = Forwarder_makefile
DDS_APP_Forwarder_Forwarder_EXE  = Forwarder
DDS_APP_Forwarder_Forwarder_SRCS = Forwarder.cpp Forwarder.hpp

DDS_APP_Forwarder_LogServer_DIR  = dds/app/Forwarder
DDS_APP_Forwarder_LogServer_MK   = LogServer_makefile
DDS_APP_Forwarder_LogServer_EXE  = LogServer
DDS_APP_Forwarder_LogServer_SRCS = LogServer.cpp LogServer.hpp

DDS_APP_ThreadAnalysisDDS_DIR  = dds/app/ThreadAnalysisDDS
DDS_APP_ThreadAnalysisDDS_MK   = ThreadAnalysisDDS_makefile
DDS_APP_ThreadAnalysisDDS_EXE  = ThreadAnalysisDDS
DDS_APP_ThreadAnalysisDDS_SRCS = ThreadAnalysisDDS.cpp ThreadAnalysisDDS.hpp

.PHONY: dds dds_libs dds_apps post_build_copy build_local_libs

dds: dds_libs dds_apps

# Le librerie DDS devono essere compilate in ordine (dipendenze tra loro)
dds_libs:
	@mkdir -p $(DDS_LIB_DIR)/output
	$(MAKE) -B -C $(DDS_LIB_DIR) -f time_makefile
	$(MAKE) -B -C $(DDS_LIB_DIR) -f activity_makefile
	$(MAKE) -B -C $(DDS_LIB_DIR) -f communication_makefile

# Compila le app DDS GeometryPeriodic
dds_apps: dds_libs
	@mkdir -p dds/app/GeometryPeriodic/output
	$(MAKE) -C $(DDS_APP_GeometryBroadcastner_DIR) -f $(DDS_APP_GeometryBroadcastner_MK)
	$(MAKE) -C $(DDS_APP_GeometryBroadcastnerImmagine_DIR) -f $(DDS_APP_GeometryBroadcastnerImmagine_MK)
	$(MAKE) -C $(DDS_APP_GeometryListener_DIR) -f $(DDS_APP_GeometryListener_MK)
	@echo "[DDS] GeometryPeriodic compilato con successo"
	@echo "[DDS] Per compilare Forwarder/ThreadAnalysisDDS usare i target dedicati"

# Target opzionali per compilare le altre app DDS singolarmente
.PHONY: dds_forwarder dds_threadanalysis
dds_forwarder: dds_libs
	@mkdir -p dds/app/Forwarder/output
	$(MAKE) -C $(DDS_APP_Forwarder_Client_DIR) -f $(DDS_APP_Forwarder_Client_MK)
	$(MAKE) -C $(DDS_APP_Forwarder_DivisionServer_DIR) -f $(DDS_APP_Forwarder_DivisionServer_MK)
	$(MAKE) -C $(DDS_APP_Forwarder_Forwarder_DIR) -f $(DDS_APP_Forwarder_Forwarder_MK)
	$(MAKE) -C $(DDS_APP_Forwarder_LogServer_DIR) -f $(DDS_APP_Forwarder_LogServer_MK)

dds_threadanalysis: dds_libs
	@mkdir -p dds/app/ThreadAnalysisDDS/output
	$(MAKE) -C $(DDS_APP_ThreadAnalysisDDS_DIR) -f $(DDS_APP_ThreadAnalysisDDS_MK)

# ===========================================================================
# POST-BUILD: Copia eseguibili e sorgenti nelle cartelle del parser
# ===========================================================================
post_build_copy:
	@echo "[POST-BUILD] Copia eseguibili e sorgenti in corso..."
	@mkdir -p eseguibili sorgenti
	@# --- GeometryPeriodic ---
	@cp -f dds/app/GeometryPeriodic/GeometryBroadcastner eseguibili/ 2>/dev/null || true
	@cp -f dds/app/GeometryPeriodic/GeometryBroadcastner.cpp sorgenti/ 2>/dev/null || true
	@cp -f dds/app/GeometryPeriodic/GeometryBroadcastner.hpp sorgenti/ 2>/dev/null || true
	@# ---> AGGIUNTO: Copia GeometryBroadcastnerImmagine <---
	@cp -f dds/app/GeometryPeriodic/GeometryBroadcastnerImmagine eseguibili/ 2>/dev/null || true
	@cp -f dds/app/GeometryPeriodic/GeometryBroadcastnerImmagine.cpp sorgenti/ 2>/dev/null || true
	@cp -f dds/app/GeometryPeriodic/GeometryBroadcastnerImmagine.hpp sorgenti/ 2>/dev/null || true
	@cp -f dds/app/GeometryPeriodic/GeometryListener eseguibili/ 2>/dev/null || true
	@cp -f dds/app/GeometryPeriodic/GeometryListener.cpp sorgenti/ 2>/dev/null || true
	@cp -f dds/app/GeometryPeriodic/GeometryListener.hpp sorgenti/ 2>/dev/null || true
	@# --- Forwarder ---
	@cp -f dds/app/Forwarder/Client eseguibili/Forwarder_Client 2>/dev/null || true
	@cp -f dds/app/Forwarder/Client.cpp sorgenti/Forwarder_Client.cpp 2>/dev/null || true
	@cp -f dds/app/Forwarder/Client.hpp sorgenti/Forwarder_Client.hpp 2>/dev/null || true
	@cp -f dds/app/Forwarder/DivisionServer eseguibili/Forwarder_DivisionServer 2>/dev/null || true
	@cp -f dds/app/Forwarder/DivisionServer.cpp sorgenti/Forwarder_DivisionServer.cpp 2>/dev/null || true
	@cp -f dds/app/Forwarder/DivisionServer.hpp sorgenti/Forwarder_DivisionServer.hpp 2>/dev/null || true
	@cp -f dds/app/Forwarder/Forwarder eseguibili/Forwarder_Forwarder 2>/dev/null || true
	@cp -f dds/app/Forwarder/Forwarder.cpp sorgenti/Forwarder_Forwarder.cpp 2>/dev/null || true
	@cp -f dds/app/Forwarder/Forwarder.hpp sorgenti/Forwarder_Forwarder.hpp 2>/dev/null || true
	@cp -f dds/app/Forwarder/LogServer eseguibili/Forwarder_LogServer 2>/dev/null || true
	@cp -f dds/app/Forwarder/LogServer.cpp sorgenti/Forwarder_LogServer.cpp 2>/dev/null || true
	@cp -f dds/app/Forwarder/LogServer.hpp sorgenti/Forwarder_LogServer.hpp 2>/dev/null || true
	@# --- ThreadAnalysisDDS ---
	@cp -f dds/app/ThreadAnalysisDDS/ThreadAnalysisDDS eseguibili/ 2>/dev/null || true
	@cp -f dds/app/ThreadAnalysisDDS/ThreadAnalysisDDS.cpp sorgenti/ 2>/dev/null || true
	@cp -f dds/app/ThreadAnalysisDDS/ThreadAnalysisDDS.hpp sorgenti/ 2>/dev/null || true
	@echo "[POST-BUILD] Copia completata!"
	@echo "  eseguibili/ -> $$(ls eseguibili/ 2>/dev/null | wc -l) file"
	@echo "  sorgenti/   -> $$(ls sorgenti/ 2>/dev/null | wc -l) file"

# ===========================================================================
# COSTRUZIONE SEQUENZIALE DELLE LIBRERIE LOCALI (Evita conflitti con -j4)
# ===========================================================================
build_local_libs:
	$(MAKE) -C ../../lib -f time_makefile
	$(MAKE) -C ../../lib -f activity_makefile
	$(MAKE) -C ../../lib -f activity_dyn_makefile

# ===========================================================================

# Trova tutti i file C e C++
SRCS_C = $(wildcard sorgenti/*.c)
SRCS_CPP_ALL = $(wildcard sorgenti/*.cpp)
# Escludi file DDS che richiedono header speciali non disponibili nella cartella standard
DDS_SRCS = $(wildcard sorgenti/Geometry*.cpp) $(wildcard sorgenti/Broadcastner*.cpp) \
           $(wildcard sorgenti/Forwarder_*.cpp) $(wildcard sorgenti/ThreadAnalysisDDS*.cpp)
SRCS_CPP = $(filter-out $(DDS_SRCS), $(SRCS_CPP_ALL))

# Filtra quelli che usano il suffisso _main
SRCS_MAIN_C = $(filter %_main.c, $(SRCS_C))
SRCS_MAIN_CPP = $(filter %_main.cpp, $(SRCS_CPP))

# I rimanenti
SRCS_NORM_C = $(filter-out $(SRCS_MAIN_C), $(SRCS_C))
SRCS_NORM_CPP = $(filter-out $(SRCS_MAIN_CPP), $(SRCS_CPP))

# Estrai i nomi degli eseguibili target in eseguibili/
EXECS_MAIN_C = $(patsubst sorgenti/%_main.c,eseguibili/%,$(SRCS_MAIN_C))
EXECS_MAIN_CPP = $(patsubst sorgenti/%_main.cpp,eseguibili/%,$(SRCS_MAIN_CPP))
EXECS_NORM_C = $(patsubst sorgenti/%.c,eseguibili/%,$(SRCS_NORM_C))
EXECS_NORM_CPP = $(patsubst sorgenti/%.cpp,eseguibili/%,$(SRCS_NORM_CPP))

EXECS = $(EXECS_MAIN_C) $(EXECS_MAIN_CPP) $(EXECS_NORM_C) $(EXECS_NORM_CPP)

.PHONY: all clean print trace_% setup dds dds_libs dds_apps post_build_copy

all: setup $(EXECS) dds_apps
	$(MAKE) post_build_copy
	$(MAKE) print

setup:
	@mkdir -p eseguibili sorgenti

# Regola per compilare i file che usano il suffisso _main
eseguibili/%: sorgenti/%_main.c dds_libs build_local_libs
	$(CC) $(CFLAGS) $< $(LIBS) -o $@ $(LDFLAGS)

eseguibili/%: sorgenti/%_main.cpp dds_libs build_local_libs
	$(CXX) $(CFLAGS) $< $(LIBS) -o $@ $(LDFLAGS)

# Regole per compilare i file standard
eseguibili/%: sorgenti/%.c dds_libs build_local_libs
	$(CC) $(CFLAGS) $< $(LIBS) -o $@ $(LDFLAGS)

eseguibili/%: sorgenti/%.cpp dds_libs build_local_libs
	$(CXX) $(CFLAGS) $< $(LIBS) -o $@ $(LDFLAGS)

# Carica automaticamente le dipendenze generate
-include $(SRCS_C:.c=.d) $(SRCS_CPP:.cpp=.d)

clean:
	rm -rf eseguibili/
	rm -f sorgenti/*.o sorgenti/*.d
	@echo "[CLEAN] Pulizia delle build DDS e Librerie..."
	-$(MAKE) -C dds/app/GeometryPeriodic -f GeometryBroadcastner_makefile clean 2>/dev/null || true
	-$(MAKE) -C dds/app/GeometryPeriodic -f GeometryBroadcastnerImmagine_makefile clean 2>/dev/null || true
	-$(MAKE) -C dds/app/GeometryPeriodic -f GeometryListener_makefile clean 2>/dev/null || true
	-$(MAKE) -C dds/app/Forwarder -f Client_makefile clean 2>/dev/null || true
	-$(MAKE) -C dds/app/Forwarder -f DivisionServer_makefile clean 2>/dev/null || true
	-$(MAKE) -C dds/app/Forwarder -f Forwarder_makefile clean 2>/dev/null || true
	-$(MAKE) -C dds/app/Forwarder -f LogServer_makefile clean 2>/dev/null || true
	-$(MAKE) -C dds/app/ThreadAnalysisDDS -f ThreadAnalysisDDS_makefile clean 2>/dev/null || true
	-$(MAKE) -C $(DDS_LIB_DIR) -f time_makefile clean 2>/dev/null || true
	-$(MAKE) -C $(DDS_LIB_DIR) -f activity_makefile clean 2>/dev/null || true
	-$(MAKE) -C $(DDS_LIB_DIR) -f communication_makefile clean 2>/dev/null || true

print:
	@echo "========================================================="
	@echo " Eseguibili compilati con successo: "
	@echo "    $(EXECS)"
	@echo ""
	@echo " Eseguibili DDS copiati in eseguibili/:"
	@ls eseguibili/Geometry* eseguibili/Forwarder_* eseguibili/ThreadAnalysisDDS 2>/dev/null || echo "    (nessuno)"
	@echo "========================================================="
	@echo " Puoi tracciare un eseguibile usando lo script:"
	@echo "    ./run_trace.sh <eseguibile> (es. ./run_trace.sh app10h)"
	@echo ""
	@echo " Oppure puoi usare il target 'make trace_<nome>':"
	@echo "    make trace_app10h"
	@echo "    make trace_GeometryBroadcastner"
	@echo "========================================================="

# Regola helper per testare un eseguibile con lo script run_trace.sh
trace_%: eseguibili/%
	sudo ./run_trace.sh ./$<
