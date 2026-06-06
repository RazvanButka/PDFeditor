# PDFeditor skeleton — build system (Linux + macOS)
#
# Linux (prima compilare pe masina noua):
#   make distclean && make soap && make all
# macOS (Homebrew):
#   brew install gsoap mupdf tesseract leptonica libconfig
#   make distclean && make soap && make all
 
CC     := gcc
UNAME  := $(shell uname -s)
 
# --- platforma: un singur loc pentru diferente OS ---
 
MUPDF_LIBS_EXPLICIT := -lmupdf -lmupdf-third \
  -lfreetype -lharfbuzz -lgumbo -lz -lm -ljpeg -lopenjp2 -ljbig2dec
# Debian/Ubuntu: libmupdf.a + libmupdf-third.a (static) — vezi apt MUPDF_APT_DEPS
MUPDF_LIBS_LINUX  := -lmupdf -lmupdf-third -lfreetype -lharfbuzz -lgumbo -lz \
                       -ljpeg -lopenjp2 -ljbig2dec -lmujs
MUPDF_APT_DEPS    := zlib1g-dev libjpeg-dev libopenjp2-7-dev libjbig2dec-dev \
                     libfreetype-dev libharfbuzz-dev libgumbo-dev libmujs-dev
 
ifeq ($(UNAME),Linux)
  PLATFORM      := linux
  CFLAGS        += -D_DEFAULT_SOURCE
  SOAP_CFLAGS   += -DWITH_NO_C_LOCALE
  LEPT_LIB      := -llept
  GSOAP_INC_DIRS := /usr/include /usr/include/gsoap /usr/local/include/gsoap
  MUPDF_PC      := $(shell pkg-config --cflags mupdf 2>/dev/null)
  MUPDF_LD      := $(shell pkg-config --libs mupdf 2>/dev/null)
else ifeq ($(UNAME),Darwin)
  PLATFORM      := macos
  LEPT_LIB      := -lleptonica
  BREW_PREFIX   := $(shell brew --prefix 2>/dev/null)
  GSOAP_INC_DIRS := /opt/homebrew/include/gsoap /opt/homebrew/include \
                    /usr/local/include/gsoap $(BREW_PREFIX)/include
  ifneq ($(BREW_PREFIX),)
    MUPDF_PC      := -I$(BREW_PREFIX)/opt/mupdf/include
    LDFLAGS_EXTRA := -L$(BREW_PREFIX)/lib
    MUPDF_LD      := $(MUPDF_LIBS_EXPLICIT) $(LDFLAGS_EXTRA)
  endif
else
  PLATFORM      := $(UNAME)
  LEPT_LIB      := -lleptonica
  GSOAP_INC_DIRS := /usr/local/include/gsoap /usr/include/gsoap
endif
 
CFLAGS   ?= -g -Wall -Wextra -std=c11
CFLAGS   += $(MUPDF_PC)
INCLUDES := -Iinclude -Ibuild/generated/soap
 
GSOAP_INC := $(foreach d,$(GSOAP_INC_DIRS),$(wildcard $(d)/stdsoap2.h))
ifneq ($(GSOAP_INC),)
  INCLUDES += $(foreach h,$(GSOAP_INC),-I$(dir $(h)))
endif
 
SOAP_CFLAGS += $(shell pkg-config --cflags gsoap 2>/dev/null)
SOAP_LIBS   := $(shell pkg-config --libs gsoap 2>/dev/null)
ifeq ($(SOAP_LIBS),)
  SOAP_LIBS := -lgsoap
endif
 
ifeq ($(UNAME),Linux)
  # Nu folosi doar $(MUPDF_LD) din pkg-config — lipseste -lz, -lfreetype, ...
  MUPDF_LIB := $(MUPDF_LIBS_LINUX) $(LDFLAGS_EXTRA)
else
  ifneq ($(MUPDF_LD),)
    MUPDF_LIB := $(MUPDF_LD)
  else
    MUPDF_LIB := $(MUPDF_LIBS_EXPLICIT) $(LDFLAGS_EXTRA)
  endif
endif
 
OCR_LIB          := -ltesseract $(LEPT_LIB) $(LDFLAGS_EXTRA)
# -lm la final: libmupdf (static pe Linux) foloseste log10f etc.
LDFLAGS_SERVER   := -lpthread $(SOAP_LIBS) -lconfig $(MUPDF_LIB) $(OCR_LIB) -lpcre2-8 -lm
LDFLAGS_CLIENT   := -lpthread
LDFLAGS_ADMIN    := -lpthread -lncurses
 
BUILD_DIR  := build
OBJ_DIR    := $(BUILD_DIR)/obj
SOAP_DIR   := $(BUILD_DIR)/generated/soap
BIN_DIR    := bin
 
# --- surse ---
 
SRC_MAIN   := src/main/threeds.c
SRC_SERVER := src/server/unixds.c src/server/inetds.c src/server/soapds.c \
              src/server/pdfds.c
SRC_CORE   := src/core/proto.c src/core/proto_inet.c src/core/pdf_ocr.c src/core/gdpr_redact.c src/core/pandoc_convert.c
SRC_CLIENT := src/client/pdfclient.c src/client/pdfadmin.c src/client/inetsample.c
 
SOAP_GEN   := $(SOAP_DIR)/soapC.c $(SOAP_DIR)/soapServer.c \
              $(SOAP_DIR)/SampleServices.nsmap
 
# --- obiecte ---
 
OBJ_MAIN   := $(OBJ_DIR)/threeds.o
OBJ_SERVER := $(OBJ_DIR)/unixds.o $(OBJ_DIR)/inetds.o $(OBJ_DIR)/soapds.o \
              $(OBJ_DIR)/pdfds.o
OBJ_CORE   := $(OBJ_DIR)/proto.o $(OBJ_DIR)/proto_inet.o $(OBJ_DIR)/pdf_ocr.o $(OBJ_DIR)/gdpr_redact.o $(OBJ_DIR)/pandoc_convert.o

OBJ_SOAP   := $(OBJ_DIR)/soapC.o $(OBJ_DIR)/soapServer.o
 
OBJS_SERVER := $(OBJ_MAIN) $(OBJ_SERVER) $(OBJ_CORE) $(OBJ_SOAP)
 
.PHONY: all help info linux-first serverds pdfclient pdfadmin inetclient soap clean distclean dirs
 
all: dirs soap serverds pdfclient pdfadmin inetclient
 
# Prima build pe Linux dupa arhiva de pe Mac
linux-first: distclean soap all
 
info:
	@echo "=== PDFeditor build info ==="
	@echo "PLATFORM   = $(PLATFORM) (uname: $(UNAME))"
	@echo "CC         = $(CC)"
	@echo "CFLAGS     = $(CFLAGS)"
	@echo "SOAP_CFLAGS= $(SOAP_CFLAGS)"
	@echo "INCLUDES   = $(INCLUDES)"
	@echo "LEPT_LIB   = $(LEPT_LIB)"
	@echo "SOAP_LIBS  = $(SOAP_LIBS)"
	@echo "gSOAP hdr  = $(firstword $(GSOAP_INC))"
	@test -n "$(GSOAP_INC)" || echo "WARN: stdsoap2.h not found — install gsoap/libgsoap-dev"
	@test "$(PLATFORM)" != linux || command -v soapcpp2 >/dev/null 2>&1 || \
		echo "WARN: soapcpp2 missing — apt install gsoap"
 
dirs:
	@mkdir -p $(OBJ_DIR) $(SOAP_DIR) $(BIN_DIR) data/input data/output
 
# --- gSOAP (regenereaza pe fiecare OS; nu copia build/generated de pe alt OS) ---
 
soap: dirs $(SOAP_GEN)
 
$(SOAP_DIR)/SampleServices.nsmap: include/soap/sclient.h
	@command -v soapcpp2 >/dev/null 2>&1 || \
		{ echo "ERROR: soapcpp2 not in PATH (apt: gsoap / brew: gsoap)"; exit 1; }
	@mkdir -p $(SOAP_DIR)
	cd $(SOAP_DIR) && soapcpp2 -S -c -x ../../../include/soap/sclient.h
 
$(SOAP_DIR)/soapC.c $(SOAP_DIR)/soapServer.c: $(SOAP_DIR)/SampleServices.nsmap
	@true
 
# --- pattern rules ---
 
$(OBJ_DIR)/%.o: src/main/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
 
$(OBJ_DIR)/%.o: src/server/%.c
	$(CC) $(CFLAGS) $(INCLUDES) $(SOAP_CFLAGS) -c $< -o $@
 
$(OBJ_DIR)/%.o: src/core/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
 
$(OBJ_DIR)/%.o: src/client/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
 
$(OBJ_DIR)/soapC.o: $(SOAP_DIR)/soapC.c
	$(CC) $(CFLAGS) $(INCLUDES) $(SOAP_CFLAGS) -c $< -o $@
 
$(OBJ_DIR)/soapServer.o: $(SOAP_DIR)/soapServer.c
	$(CC) $(CFLAGS) $(INCLUDES) $(SOAP_CFLAGS) -c $< -o $@
 
# --- binare ---
 
serverds: dirs soap $(BIN_DIR)/serverds
 
$(BIN_DIR)/serverds: $(OBJS_SERVER)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_SERVER)
 
pdfclient: dirs $(OBJ_DIR)/proto.o $(BIN_DIR)/pdfclient
 
$(BIN_DIR)/pdfclient: $(OBJ_DIR)/pdfclient.o $(OBJ_DIR)/proto.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_CLIENT)
 
pdfadmin: dirs $(OBJ_DIR)/proto.o $(BIN_DIR)/pdfadmin
 
$(BIN_DIR)/pdfadmin: $(OBJ_DIR)/pdfadmin.o $(OBJ_DIR)/proto.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_ADMIN)
 
inetclient: dirs $(BIN_DIR)/inetclient
 
$(BIN_DIR)/inetclient: $(OBJ_DIR)/inetsample.o $(OBJ_DIR)/proto_inet.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_CLIENT)
 
# --- curatare ---
 
clean:
	rm -rf $(OBJ_DIR)/*
	rm -f $(BIN_DIR)/serverds $(BIN_DIR)/pdfclient $(BIN_DIR)/pdfadmin $(BIN_DIR)/inetclient
 
distclean: clean
	rm -rf $(SOAP_DIR)/*
 
help:
	@echo "PDFeditor skeleton — comenzi make"
	@echo ""
	@echo "  make info           — OS detectat + flags (debug Linux/Mac)"
	@echo "  make / make all     — compileaza server + clienti"
	@echo "  make linux-first    — distclean + soap + all (Linux dupa zip Mac)"
	@echo "  make soap           — regenereaza gSOAP (obligatoriu pe OS nou)"
	@echo "  make clean          — sterge obiecte si binare"
	@echo "  make distclean      — clean + sterge SOAP generat"
	@echo ""
	@echo "Platforma curenta: $(PLATFORM) ($(UNAME))"
	@echo ""
	@echo "Linux (Ubuntu/Debian):"
	@echo "  sudo apt install build-essential gsoap libgsoap-dev libconfig-dev \\"
	@echo "    libmupdf-dev libtesseract-dev libleptonica-dev libncurses-dev pkg-config \\"
	@echo "    $(MUPDF_APT_DEPS)"
	@echo "  make linux-first"
	@echo ""
	@echo "macOS (Homebrew):"
	@echo "  brew install gsoap mupdf tesseract leptonica libconfig"
	@echo "  make distclean && make soap && make all"