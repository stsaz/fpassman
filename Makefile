
ROOT := ..
FPASSMAN := $(ROOT)/fpassman
FFBASE := $(ROOT)/ffbase
FFOS := $(ROOT)/ffos
FFPACK := $(ROOT)/ffpack

include $(FFBASE)/test/makeconf

SUBMAKE := $(MAKE) -f $(firstword $(MAKEFILE_LIST))

FFPACK_BIN := $(FFPACK)/_$(OS)-$(CPU)
CRYPTOLIB3 := $(FPASSMAN)/cryptolib3
CRYPTOLIB3_BIN := $(FPASSMAN)/cryptolib3/_$(OS)-amd64

# OS-specific options
BIN := fpassman
INST_DIR := fpassman-1
ifeq "$(OS)" "windows"
	BIN := fpassman.exe
	CFLAGS += -DFF_WIN_APIVER=0x0501
endif

CFLAGS += -fno-strict-aliasing -fvisibility=hidden \
	-Wall -Wno-multichar \
	-MMD -MP \
	-I$(FPASSMAN)/src -I$(FFOS) -I$(FFBASE) -I$(FFPACK) -I$(CRYPTOLIB3)
ifeq "$(DEBUG)" "1"
	CFLAGS += -O0 -g -DFF_DEBUG -Werror
else
	CFLAGS += -O3
endif
CXXFLAGS := $(CFLAGS) -fno-exceptions -fno-rtti -Wno-c++11-narrowing -Wno-sign-compare
LINKFLAGS += \
	-fvisibility=hidden -L$(FFPACK_BIN)


OSBINS :=
ifeq "$(OS)" "windows"
	OSBINS := fpassman-gui.exe
endif

build: $(BIN) $(OSBINS)

-include $(wildcard *.d)

OBJ := \
	db.o dbf-json.o \
	core.o \
	sha256.o sha256-ff.o sha1.o

%.o: $(FPASSMAN)/src/%.c
	$(C) $(CFLAGS) $< -o $@

%.o: $(CRYPTOLIB3)/sha/%.c
	$(C) $(CFLAGS) $< -o $@
%.o: $(CRYPTOLIB3)/sha1/%.c
	$(C) $(CFLAGS) $< -o $@


#
%.o: $(FPASSMAN)/src/gui/%.cpp
	$(CXX) $(CXXFLAGS) $< -o $@
%.o: $(FPASSMAN)/src/util/gui-winapi/%.c
	$(C) $(CFLAGS) $< -o $@

fpassman.coff: $(FPASSMAN)/fpassman.rc $(FPASSMAN)/fpassman.ico
	$(WINDRES) -I$(FPASSMAN)/src -I$(FFOS) -I$(FFBASE) $(FPASSMAN)/fpassman.rc $@

BINGUI_O := $(OBJ) \
	gui.o \
	ffgui-winapi-loader.o \
	ffgui-winapi.o \
	fpassman.coff \
	$(CRYPTOLIB3_BIN)/AES.a
fpassman-gui.exe: $(BINGUI_O)
	$(LINKXX) $+ $(LINKFLAGS) -lshell32 -luxtheme -lcomctl32 -lcomdlg32 -lgdi32 -lws2_32 -lole32 -luuid -lz-ffpack -mwindows -o $@


#
BIN_O = $(OBJ) \
	tui.o \
	$(CRYPTOLIB3_BIN)/AES.a
ifeq "$(OS)" "windows"
	BIN_O += fpassman.coff
endif
$(BIN): $(BIN_O)
	$(LINK) $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -lz-ffpack -o $@


clean:
	rm -f $(BIN) $(OSBINS) *.debug $(OBJ) fpassman.coff

strip:
	strip $(BIN) $(OSBINS)

install:
	mkdir -vp $(INST_DIR)
	$(CP) \
		$(FFPACK_BIN)/libz-ffpack.$(SO) \
		$(BIN) \
		$(FPASSMAN)/fpassman.conf $(FPASSMAN)/help.txt \
		$(INST_DIR)/
	$(CP) $(FPASSMAN)/README.md $(INST_DIR)/README.txt

ifeq "$(OS)" "windows"
	$(CP) \
		fpassman-gui.exe \
		$(FPASSMAN)/src/gui/fpassman.gui \
		$(INST_DIR)/
	unix2dos $(INST_DIR)/*.txt $(INST_DIR)/*.conf $(INST_DIR)/*.gui
endif

build-install: build
	$(SUBMAKE) install

strip-install: build
	$(SUBMAKE) strip
	$(SUBMAKE) install

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
	$(SUBMAKE) package
