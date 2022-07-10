
PROJ := fpassman
ROOT := ..
PROJDIR := $(ROOT)/fpassman
SRCDIR := $(PROJDIR)/src
OBJ_DIR := .
FFBASE := $(ROOT)/ffbase
FFOS := $(ROOT)/ffos
FFPACK := $(ROOT)/ffpack

include $(FFOS)/test/makeconf2

CRYPTOLIB3 := $(PROJDIR)/cryptolib3/_$(OS)-amd64

# OS-specific options
ifeq "$(OS)" "windows"
BIN := fpassman.exe
INST_DIR := fpassman
CFLAGS += -DFF_WIN_APIVER=0x0501

else
BIN := fpassman
INST_DIR := fpassman-1
endif

CFLAGS += -O2 -g -fno-strict-aliasing -fvisibility=hidden \
	-Wall -Werror \
	-I$(SRCDIR) -I$(FFOS) -I$(FFBASE) -I$(FFPACK) -I$(CRYPTOLIB3)/..

LINKFLAGS += \
	-fvisibility=hidden -L$(FFPACK)/zlib


OSBINS :=
ifeq "$(OS)" "windows"
OSBINS := fpassman-gui.exe
endif

build: $(BIN) $(OSBINS)

HDRS := $(PROJDIR)/Makefile \
	$(wildcard $(SRCDIR)/*.h) \
	$(wildcard $(SRCDIR)/util/*.h) \
	$(wildcard $(FFBASE)/*.h) \
	$(wildcard $(FFOS)/*.h)

OBJ := \
	$(OBJ_DIR)/db.o $(OBJ_DIR)/dbf-json.o \
	$(OBJ_DIR)/core.o \
	$(OBJ_DIR)/sha256.o $(OBJ_DIR)/sha256-ff.o $(OBJ_DIR)/sha1.o

$(OBJ_DIR)/%.o: $(SRCDIR)/%.c $(HDRS)
	$(C) $(CFLAGS) $< -o $@

$(OBJ_DIR)/%.o: $(CRYPTOLIB3)/../sha/%.c
	$(C) $(CFLAGS) $< -o $@
$(OBJ_DIR)/%.o: $(CRYPTOLIB3)/../sha1/%.c
	$(C) $(CFLAGS) $< -o $@


#
$(OBJ_DIR)/%.o: $(SRCDIR)/gui/%.c $(HDRS)
	$(C) $(CFLAGS) $< -o $@
$(OBJ_DIR)/%.o: $(SRCDIR)/util/gui-winapi/%.c $(HDRS)
	$(C) $(CFLAGS) $< -o $@

$(OBJ_DIR)/fpassman.coff: $(PROJDIR)/fpassman.rc $(PROJDIR)/fpassman.ico
	$(WINDRES) -I$(SRCDIR) -I$(FFOS) -I$(FFBASE) $(PROJDIR)/fpassman.rc $@

BINGUI_O := $(OBJ) \
	$(OBJ_DIR)/gui.o \
	$(OBJ_DIR)/ffgui-winapi-loader.o \
	$(OBJ_DIR)/ffgui-winapi.o \
	$(OBJ_DIR)/fpassman.coff \
	$(CRYPTOLIB3)/AES-ff.a
fpassman-gui.exe: $(BINGUI_O)
	$(LINK) $(LINKFLAGS) $+ -lshell32 -luxtheme -lcomctl32 -lcomdlg32 -lgdi32 -lws2_32 -lole32 -luuid -lz-ff -mwindows -o $@


#
BIN_O = $(OBJ) \
	$(OBJ_DIR)/tui.o \
	$(CRYPTOLIB3)/AES-ff.a
ifeq "$(OS)" "windows"
BIN_O += $(OBJ_DIR)/fpassman.coff
endif
$(BIN): $(BIN_O)
	$(LINK) $(LINKFLAGS) $(LINK_RPATH_ORIGIN) $+ -lz-ff -o $@


clean:
	rm -f $(BIN) $(OSBINS) *.debug $(OBJ) $(OBJ_DIR)/fpassman.coff

strip:
	strip $(BIN) $(OSBINS)

install:
	mkdir -vp $(INST_DIR)
	$(CP) \
		$(FFPACK)/zlib/libz-ff.$(SO) \
		$(BIN) \
		$(PROJDIR)/fpassman.conf $(PROJDIR)/help.txt \
		$(INST_DIR)/
	$(CP) $(PROJDIR)/README.md $(INST_DIR)/README.txt

ifeq "$(OS)" "windows"
	$(CP) \
		fpassman-gui.exe \
		$(PROJDIR)/src/gui/fpassman.gui \
		$(INST_DIR)/
	unix2dos $(INST_DIR)/*.txt $(INST_DIR)/*.conf $(INST_DIR)/*.gui
endif

build-install: build
	$(MAKE) -f $(firstword $(MAKEFILE_LIST)) install

strip-install: build
	$(MAKE) -f $(firstword $(MAKEFILE_LIST)) strip
	$(MAKE) -f $(firstword $(MAKEFILE_LIST)) install

# package
PKG_VER := 0.1
PKG_ARCH := amd64
PKG_PACKER := tar -c --owner=0 --group=0 --numeric-owner -v --zstd -f
PKG_EXT := tar.zst
ifeq "$(OS)" "windows"
	PKG_ARCH := x64
	PKG_PACKER := zip -r -v
	PKG_EXT := zip
endif
package:
	$(PKG_PACKER) fpassman-$(PKG_VER)-$(OS)-$(PKG_ARCH).$(PKG_EXT) $(INST_DIR)

install-package: strip-install
	$(MAKE) -f $(firstword $(MAKEFILE_LIST)) package
