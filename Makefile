
PROJ := fpassman
ROOT := ..
PROJDIR := $(ROOT)/fpassman
SRCDIR := $(PROJDIR)/src
VER :=
OS :=
OPT := LTO3

FFOS := $(ROOT)/ffos
FF := $(ROOT)/ff
FF3PT := $(ROOT)/ff-3pt

include $(FFOS)/makeconf


# OS-specific options
ifeq ($(OS),win)
BIN := fpassman.exe
INSTDIR := fpassman
CFLAGS += -DFF_WIN=0x0501

else
BIN := fpassman
INSTDIR := fpassman-1
endif


FF_OBJ_DIR := ./ff-obj
FFOS_CFLAGS := $(CFLAGS) -pthread
FF_CFLAGS := $(CFLAGS)
FF3PTLIB := $(FF3PT)-bin/$(OS)-$(ARCH)
FF3PT_CFLAGS := $(CFLAGS)


CFLAGS += $(ALL_CFLAGS) -Wall -Werror -pthread \
	-I$(SRCDIR) -I$(FF) -I$(FFOS) -I$(FF3PT)

LDFLAGS += -pthread \
	-fvisibility=hidden -Wl,-gc-sections -L$(FF3PTLIB)

# 3-party libraries
ZLIB := -lz

include $(PROJDIR)/makerules

package:
	rm -f $(PROJ)-$(VER)-$(OS)-$(ARCH_OS).$(PACK_EXT) \
		&&  $(PACK) $(PROJ)-$(VER)-$(OS)-$(ARCH_OS).$(PACK_EXT) $(INSTDIR)
	$(PACK) $(PROJ)-$(VER)-$(OS)-$(ARCH_OS)-debug.$(PACK_EXT) ./*.debug
