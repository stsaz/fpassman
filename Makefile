
ROOT := ..
FPASSMAN := $(ROOT)/fpassman
FFBASE := $(ROOT)/ffbase
FFSYS := $(ROOT)/ffsys
FFGUI := $(ROOT)/ffgui
FFPACK := $(ROOT)/ffpack

include $(FFBASE)/conf.mk

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
	-DFFBASE_MEM_ASSERT -DFFBASE_HAVE_FFERR_STR \
	-MMD -MP \
	-I$(FPASSMAN)/src -I$(FFSYS) -I$(FFBASE) -I$(FFPACK) -I$(CRYPTOLIB3)
ifeq "$(DEBUG)" "1"
	CFLAGS += -O0 -g -DFF_DEBUG -Werror
else
	CFLAGS += -O3
endif
CXXFLAGS := $(CFLAGS) -fno-exceptions -fno-rtti -Wno-c++11-narrowing -Wno-sign-compare
LINKFLAGS += \
	-fvisibility=hidden -L$(FFPACK_BIN)


OSBINS := fpassman-gui
ifeq "$(OS)" "windows"
	OSBINS := fpassman-gui.exe
endif

default: build
ifneq "$(DEBUG)" "1"
	$(SUBMAKE) strip
endif
	$(SUBMAKE) install
	$(SUBMAKE) package

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


exe.coff: $(FPASSMAN)/res/exe.rc $(FPASSMAN)/res/fpassman.ico
	$(WINDRES) $< $@

include $(FPASSMAN)/src/gui/Makefile


#
BIN_O = $(OBJ) \
	tui.o \
	$(CRYPTOLIB3_BIN)/AES.a
ifeq "$(OS)" "windows"
	BIN_O += exe.coff
endif
$(BIN): $(BIN_O)
	$(LINK) $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -lz-ffpack -o $@


clean:
	rm -f $(BIN) $(OSBINS) *.debug $(OBJ) exe.coff

strip:
	strip $(BIN) $(OSBINS)

install:
	mkdir -vp $(INST_DIR)
	$(CP) \
		$(FFPACK_BIN)/libz-ffpack.$(SO) \
		$(BIN) \
		$(FPASSMAN)/fpassman.conf \
		$(INST_DIR)/
	$(CP) $(FPASSMAN)/README.md $(INST_DIR)/README.txt

ifeq "$(OS)" "windows"
	$(CP) fpassman-gui.exe $(INST_DIR)/
	$(CP) $(FPASSMAN)/src/gui/ui-winapi.conf $(INST_DIR)/fpassman.gui
	unix2dos $(INST_DIR)/*.txt $(INST_DIR)/*.conf $(INST_DIR)/*.gui
else
	$(CP) fpassman-gui $(INST_DIR)/
	$(CP) $(FPASSMAN)/src/gui/ui-gtk.conf $(INST_DIR)/fpassman.gui
endif

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

release: default
	$(SUBMAKE) package
