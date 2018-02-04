HUGO_VERSION=1.1.2
TARGET = hugo

CROSS_COMPILE ?= mipsel-linux-
CC          := $(CROSS_COMPILE)gcc
STRIP       := $(CROSS_COMPILE)strip

SYSROOT     := $(shell $(CC) --print-sysroot)
SDL_CFLAGS  := $(shell $(SYSROOT)/usr/bin/sdl-config --cflags)
SDL_LIBS    := $(shell $(SYSROOT)/usr/bin/sdl-config --libs)

OBJS = gp2x_psp.o \
  cpudingux.o \
  Hugo.o \
  psp_main.o \
  psp_sdl.o \
  psp_font.o \
  psp_kbd.o \
  psp_menu.o \
  psp_danzeff.o \
  psp_menu_set.o \
  psp_menu_help.o \
  psp_menu_cheat.o \
  psp_menu_list.o \
  psp_menu_kbd.o \
  psp_menu_joy.o \
  psp_editor.o \
  psp_fmgr.o \
  psp_audio.o \
  psp_joy.o \
  core/psp/osd_psp_cd.o \
  core/psp/osd_psp_mix.o \
  core/psp/osd_psp_gfx.o \
  core/psp/osd_psp_snd.o \
  core/psp/osd_psp_machine.o \
  core/psp/osd_psp_keyboard.o \
  core/cd.o \
  core/dis.o \
  core/edit_ram.o \
  core/followop.o \
  core/format.o \
  core/gfx.o \
  core/h6280.o \
  core/hard_pce.o \
  core/hugo.o \
  core/list_rom.o \
  core/lsmp3.o \
  core/mix.o \
  core/optable.o \
  core/pce.o \
  core/pcecd.o \
  core/sound.o \
  core/sprite.o \
  core/utils.o \
  core/hcd.o  \
  core/miniunz.o  \
  core/unzip.o  \
  core/bios.o core/cheat.o core/debug.o core/iniconfig.o core/bp.o

DEFAULT_CFLAGS = -I. \
  -Icore -Icore/psp \
  -DDINGUX_MODE -DHUGO_VERSION=\"$(HUGO_VERSION)\" \
  -O3 -D_GNU_SOURCE=1 -D_REENTRANT -DIS_LITTLE_ENDIAN -DFINAL_RELEASE \
  -fsigned-char -ffast-math -fomit-frame-pointer \
  -fexpensive-optimizations -fno-strength-reduce 
  #-DDINGUX_MODE -DNO_STDIO_REDIRECT -DMPU_JZ4740 -DHUGO_VERSION=\"$(HUGO_VERSION)\" \
  # -funroll-loops  -finline-functions 
  # -DHUGO_OPTIM_IDLE
  # -ffast-math -fomit-frame-pointer -fno-strength-reduce 
  #  -funroll-loops  -finline-functions
  # -funroll-loops -ffast-math -fomit-frame-pointer -fno-strength-reduce -finline-functions \
  # -pipe -D_GNU_SOURCE=1 -D_REENTRANT -DIS_LITTLE_ENDIAN \
  -DUNIX -DBPP16 -DLSB_FIRST -DSOUND  -DNO_STDIO_REDIRECT  \
  -DDINGUX_MODE -O2

CFLAGS = $(DEFAULT_CFLAGS) $(SDL_CFLAGS) 
LDFLAGS = $(SDL_LIBS) -lSDL_image -lpng -lz

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $(OBJS) -o $(TARGET) && $(STRIP) $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

ctags: 
	ctags *[ch]

