TARGET=retrogame
DEBUG=1

ifeq ($(DEBUG),0)
  CROSS_COMPILE=mipsel-linux-
endif

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
STRIP = $(CROSS_COMPILE)strip

SYSROOT     := $(shell $(CC) --print-sysroot)
SDL_CFLAGS  := $(shell $(SYSROOT)/usr/bin/sdl-config --cflags)
SDL_LIBS    := $(shell $(SYSROOT)/usr/bin/sdl-config --libs)

CFLAGS = -DTARGET_RETROGAME -DTARGET=$(TARGET) -DLOG_LEVEL=3 -Wall -Wundef -Wno-deprecated -Wno-unknown-pragmas -Wno-format -g3 $(SDL_CFLAGS) -I/home/steward/Github/gh_retrogame_toolchain/mipsel-linux-gcc/usr/mipsel-buildroot-linux-uclibc/sysroot/usr/include/SDL/
CXXFLAGS = $(CFLAGS)
LDFLAGS = $(SDL_LIBS) -lfreetype -lSDL_image -lSDL_ttf -lSDL_gfx -lSDL

OBJDIR = objs/$(TARGET)
DISTDIR = dist/$(TARGET)/gmenu2x
APPNAME = $(OBJDIR)/gmenu2x

SOURCES := $(wildcard src/*.cpp)
OBJS := $(patsubst src/%.cpp, $(OBJDIR)/src/%.o, $(SOURCES))

# File types rules
$(OBJDIR)/src/%.o: src/%.cpp src/%.h
	$(CXX) $(CFLAGS) -o $@ -c $<

all: dir shared

dir:
	@if [ ! -d $(OBJDIR)/src ]; then mkdir -p $(OBJDIR)/src; fi

debug: $(OBJS)
	@echo "Linking gmenu2x-debug..."
	$(CXX) -o $(APPNAME)-debug $(LDFLAGS) $(OBJS) $(LDFLAGS) 

shared: debug
	$(STRIP) $(APPNAME)-debug -o $(APPNAME)

clean:
	rm -rf $(OBJDIR) $(DISTDIR) *.gcda *.gcno $(APPNAME)

dist: dir shared
	install -m755 -D $(APPNAME)-debug $(DISTDIR)/gmenu2x
ifeq ($(DEBUG),0)
	install -m644 assets/$(TARGET)/input.conf $(DISTDIR)
else
	install -m644 assets/pc/input.conf $(DISTDIR)
endif
	install -m755 -d $(DISTDIR)/sections/applications $(DISTDIR)/sections/emulators $(DISTDIR)/sections/games $(DISTDIR)/sections/settings
	install -m644 -D README.rst $(DISTDIR)/README.txt
	install -m644 -D COPYING $(DISTDIR)/COPYING
	install -m644 -D ChangeLog $(DISTDIR)/ChangeLog
	cp -RH assets/skins assets/translations $(DISTDIR)
	cp -RH assets/$(TARGET)/BlackJeans.png $(DISTDIR)/skins/Default/wallpapers
	cp -RH assets/$(TARGET)/skin.conf $(DISTDIR)/skins/Default
	cp -RH assets/$(TARGET)/font.ttf $(DISTDIR)/skins/Default
	cp -RH assets/$(TARGET)/gmenu2x.conf $(DISTDIR)
	cp -RH assets/$(TARGET)/icons/* $(DISTDIR)/skins/Default/icons/
	cp -RH assets/$(TARGET)/emulators/* $(DISTDIR)/sections/emulators/
	cp -RH assets/$(TARGET)/games/* $(DISTDIR)/sections/games/

-include $(patsubst src/%.cpp, $(OBJDIR)/src/%.d, $(SOURCES))

$(OBJDIR)/src/%.d: src/%.cpp
	@if [ ! -d $(OBJDIR)/src ]; then mkdir -p $(OBJDIR)/src; fi
	$(CXX) -M $(CXXFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$
