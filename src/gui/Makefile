ifeq "$(OS)" "windows"
	CFLAGS_GUI := -Wno-missing-field-initializers
	LINKFLAGS_GUI := -lshell32 -luxtheme -lcomctl32 -lcomdlg32 -lgdi32 -lole32 -luuid
	ifneq "$(DEBUG)" "1"
		LINKFLAGS_GUI += -mwindows
	endif
else
	CFLAGS_GUI := -Wno-free-nonheap-object -Wno-deprecated-declarations \
		$(shell pkg-config --cflags gtk+-3.0)
	LINKFLAGS_GUI := $(shell pkg-config --libs gtk+-3.0) \
		$(LINK_PTHREAD) -lm
endif

ifeq "$(DEBUG_GUI)" "1"
	CFLAGS_GUI += -DFFGUI_DEBUG
endif

%.o: $(FFGUI)/ffgui/winapi/%.c
	$(C) $(CFLAGS) $(CFLAGS_GUI) -I$(FFGUI) $< -o $@

%.o: $(FFGUI)/ffgui/gtk/%.c
	$(C) $(CFLAGS) $(CFLAGS_GUI) -I$(FFGUI) $< -o $@

%.o: $(FPASSMAN)/src/gui/%.cpp
	$(CXX) $(CXXFLAGS) $(CFLAGS_GUI) -I$(FFGUI) $< -o $@

fpassman-gui.exe: $(OBJ) \
		gui.o \
		ffgui-winapi-loader.o \
		ffgui-winapi.o \
		exe.coff \
		$(CRYPTOLIB3_BIN)/AES.a
	$(LINKXX) $+ $(LINKFLAGS) $(LINKFLAGS_GUI) -lz-ffpack -o $@

fpassman-gui: $(OBJ) \
		gui.o \
		ffgui-gtk-loader.o \
		ffgui-gtk.o \
		$(CRYPTOLIB3_BIN)/AES.a
	$(LINKXX) $+ $(LINKFLAGS) $(LINKFLAGS_GUI) $(LINK_RPATH_ORIGIN) -lz-ffpack -o $@
