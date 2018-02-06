#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int   u32;

#include "gp2x_psp.h"
#include "gp2x_cpu.h"
#include <time.h>
#include <dirent.h>

# ifndef CLK_TCK
# define CLK_TCK  CLOCKS_PER_SEC
# endif

#include <SDL.h>

#include "Hugo.h"

/* Resolution: 256x212, 320x256 and 512x256  */

# if 0 //LUDO:
#define HUGO_MAX_WIDTH 	(536 + 32 + 32)
#define	HUGO_MAX_HEIGHT	(240 + 64 + 64)
# else
#define HUGO_MAX_WIDTH 	(320 + 64)
#define	HUGO_MAX_HEIGHT	(256 + 64)
# endif

#define HUGO_WIDTH  256
#define HUGO_HEIGHT 212

# define SNAP_WIDTH   128
# define SNAP_HEIGHT  (106*2)

#endif
