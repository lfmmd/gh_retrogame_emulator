DEBUG=0

ifeq ($(DEBUG),0)
  CROSS_COMPILE = mipsel-linux-
endif

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
TARGET = jysdllua.dge
BASEFLAGS = -g3 -D_DEBUG=0
OBJ = charset.o jymain.o luafun.o mainmap.o piccache.o sdlfun.o
HEADERS = config.h list.h luafun.h

SYSROOT     := $(shell $(CC) --print-sysroot)
SDL_CFLAGS  := $(shell $(SYSROOT)/usr/bin/sdl-config --cflags)
SDL_LIBS    := $(shell $(SYSROOT)/usr/bin/sdl-config --libs)

CFLAGS = ${BASEFLAGS} $(SDL_CFLAGS)
ifeq ($(DEBUG),1)
  CFLAGS += -I/usr/include/lua5.1/
endif

LDFLAGS = ${BASEFLAGS} $(SDL_LIBS) -lSDL_image -lSDL_mixer -lSDL_ttf
ifeq ($(DEBUG),1)
  LDFLAGS += -llua5.1
else
  LDFLAGS += -llua
endif

#CFLAGS += -DHAS_SDL_MPEG=1
#LDFLAGS += -lsmpeg

all: ${TARGET}

${TARGET}: ${OBJ}
	${CXX} ${LDFLAGS} -o ${TARGET} ${OBJ}

clean:
	rm -f *.o ${TARGET}

distclean:
	rm -f *.o ${TARGET}

%.o:    %.cpp ${HEADERS}
	${CXX} ${CFLAGS} -c $< -o $@

%.o:    %.c ${HEADERS}
	${CC} ${CFLAGS} -c $< -o $@

