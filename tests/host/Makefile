# Host-side GUI test harness.
#
# Builds the portable GUI sources (the same files that compile into the kernel)
# against a shim layer and native libc, with ASan/UBSan. The shim include root
# takes priority so <core/memory.h>, <debug.h> and <core/Iguard.h> resolve to
# host stubs; the real project headers are added with -idirafter so they do not
# shadow the C library's <string.h>/<stdlib.h>/<stdint.h>.

CXX      ?= g++
ROOT     := ../..
SHIM     := shim

CXXFLAGS := -m64 -std=c++20 -g -O1 -fno-exceptions -fno-rtti -fno-omit-frame-pointer \
            -DKDBG_ENABLE=0 -DGUI_HOST_BUILD=1 \
            -I$(SHIM) -I. -idirafter $(ROOT)/include -idirafter $(ROOT) \
            -fsanitize=address,undefined

LDFLAGS  := -fsanitize=address,undefined -lm

SRCS := main.cpp \
        shim/kmem_host.cpp \
        shim/guiplat_host.cpp \
        shim/widgetHostTest.cpp \
        $(ROOT)/gui/widget.cpp \
        $(ROOT)/gui/window.cpp \
        $(ROOT)/gui/button.cpp \
        $(ROOT)/gui/iconbutton.cpp \
        $(ROOT)/gui/label.cpp \
        $(ROOT)/gui/listview.cpp \
        $(ROOT)/gui/inputbox.cpp \
        $(ROOT)/gui/progressbar.cpp \
        $(ROOT)/gui/elements/window_action_button.cpp \
        $(ROOT)/gui/elements/window_action_button_round.cpp \
        $(ROOT)/gui/render/raster.cpp \
        $(ROOT)/gui/renderer/nina.cpp \
        $(ROOT)/gui/fonts/font.cpp \
        $(ROOT)/core/fonts/ttf_render.cpp

TARGET := gui-test

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET)
