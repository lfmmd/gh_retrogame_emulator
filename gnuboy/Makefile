CC = mipsel-linux-gcc
LD = $(CC)
AS = $(CC)

CFLAGS = -I/home/steward/Downloads/buildroot-2017.02.9/output/host/usr/mipsel-buildroot-linux-uclibc/sysroot/usr/include/SDL/ -I/home/steward/Downloads/buildroot-2017.02.9/output/host/usr/mipsel-buildroot-linux-uclibc/sysroot/usr/include/ -I. -ansi -pedantic -Wall -Wno-implicit -Wno-long-long -O3 -DALLOW_UNALIGNED_IO -fstrength-reduce -fthread-jumps  -fcse-follow-jumps -fcse-skip-blocks -frerun-cse-after-loop  -fexpensive-optimizations -fforce-addr -fomit-frame-pointer
#LDFLAGS = $(CFLAGS) -static -s
LDFLAGS = $(CFLAGS) -s -lSDL
ASFLAGS = $(CFLAGS)

TARGETS =  sdlgnuboy

ASM_OBJS = 

SYS_DEFS = -DHAVE_CONFIG_H -DIS_LITTLE_ENDIAN  -DIS_LINUX
SYS_OBJS = sys/nix/nix.o $(ASM_OBJS)
SYS_INCS = -I/opt/dingoo-uclibc-toolchain-05-28-09/usr/include  -I./sys/nix 
FB_OBJS =  sys/linux/joy.o sys/oss/oss.o
FB_LIBS = 

SVGA_OBJS = sys/svga/svgalib.o sys/pc/keymap.o sys/linux/joy.o sys/oss/oss.o
SVGA_LIBS = -lvga

SDL_OBJS = sys/sdl/sdl.o sys/sdl/keymap.o
SDL_LIBS = -ldl -lpthread -lm -lrt
SDL_CFLAGS = -D_GNU_SOURCE=1 -D_REENTRANT

X11_OBJS = sys/x11/xlib.o sys/x11/keymap.o sys/linux/joy.o sys/oss/oss.o
X11_LIBS =  -lX11 -lXext

all: $(TARGETS)

include Rules

fbgnuboy: $(OBJS) $(SYS_OBJS) $(FB_OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(SYS_OBJS) $(FB_OBJS) -o $@ $(FB_LIBS)

sgnuboy: $(OBJS) $(SYS_OBJS) $(SVGA_OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(SYS_OBJS) $(SVGA_OBJS) -o $@ $(SVGA_LIBS)

sdlgnuboy: $(OBJS) $(SYS_OBJS) $(SDL_OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(SYS_OBJS) $(SDL_OBJS) -o $@ $(SDL_LIBS)

sys/sdl/sdl.o: sys/sdl/sdl.c
	$(CC) $(SDL_CFLAGS) -c $< -o $@

sys/sdl/keymap.o: sys/sdl/keymap.c
	$(CC) $(SDL_CFLAGS) -c $< -o $@

xgnuboy: $(OBJS) $(SYS_OBJS) $(X11_OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(SYS_OBJS) $(X11_OBJS) -o $@ $(X11_LIBS)

install: all
	$(INSTALL) -d $(bindir)
	$(INSTALL) -m 755 $(TARGETS) $(bindir)

clean:
	rm -f *gnuboy gmon.out *.o sys/*.o sys/*/*.o asm/*/*.o

distclean: clean
	rm -f config.* sys/nix/config.h Makefile




