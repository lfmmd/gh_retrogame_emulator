/*
 *  Copyright (C) 2009 Ludovic Jacomme (ludovic.jacomme@gmail.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "global.h"
#include "Hugo.h"
#include "miniunz.h"
#include "psp_sdl.h"
#include "psp_kbd.h"
#include "psp_joy.h"
#include "psp_menu.h"
#include "psp_fmgr.h"

#include "hard_pce.h"

  Hugo_t HUGO;

  int psp_screenshot_mode = 0;

extern UChar* osd_gfx_buffer;
extern UInt16 osd_pal[256];

/* Resolution: 256x212, 320x256 and 512x256  */
void
hugo_save_blit_image()
{
  u8*  src = osd_gfx_buffer;
  u32* tgt = blit_surface->pixels;
  u32 screen_w = io.screen_w;
  if (screen_w > blit_surface->w) screen_w = blit_surface->w;
  int h = 240;
  while (h-- > 0) {
    int w = screen_w / 4;
    u32 v;
    while (w-- > 0) {
      v = osd_pal[ *src++ ];
      v |= osd_pal[ *src++ ] << 16;
      *tgt++ = v;
      v = osd_pal[ *src++ ];
      v |= osd_pal[ *src++ ] << 16;
      *tgt++ = v;
    }
    tgt += (blit_surface->w - screen_w) / 2;
    src += (HUGO_MAX_WIDTH - screen_w);
  }
}

static void
loc_put_image_fit()
{
  hugo_save_blit_image();

#if 1
  SDL_Rect srcRect;
  SDL_Rect dstRect;

  srcRect.w = io.screen_w;
  srcRect.h = io.screen_h;
  srcRect.x = 0;
  srcRect.y = 0;

  dstRect.w = 320;
  dstRect.h = 480;
  dstRect.x = 0;
  dstRect.y = 0;

  SDL_SoftStretch( blit_surface, &srcRect, back_surface, &dstRect );
#else
	// will crash
  // for retrogame device
	int x, y;
  uint32_t *src = blit_surface->pixels;
  uint32_t *dst = back_surface->pixels;

  for(y=0; y<240; y++){
    for(x=0; x<160; x++){
      *dst++ = *src++;
    }
    dst+= 160;
  }
#endif
}

static void
loc_put_image_stretch_fast()
{
  u8*  src = osd_gfx_buffer;
  u32* tgt = back_surface->pixels;
  u32  screen_h = io.screen_h;
  u32  screen_w = io.screen_w;

  // screen_w > 320 
  u32 step_max = (320 / ( screen_w - 320 ));
  if (screen_h <= 240) {
    tgt += 160 * (( 240 - screen_h ) / 2);
  } else {
    src += HUGO_MAX_WIDTH * ((screen_h - 240) / 2);
    screen_h = 240;
  }
	
	printf("%s, %d %d\n", __func__, screen_h, screen_w);
  while (screen_h-- > 0) {
    u32 w = 320 / 4;
    u32 v;
    u32 step = step_max;
    u8 *start_src = src;
    while (w-- > 0) {
      v = osd_pal[ *src++ ];
      v |= osd_pal[ *src++ ] << 16;
      *tgt++ = v;
      step -= 2;
      if (step <= 0) { src++; step += step_max; }
      v = osd_pal[ *src++ ];
      v |= osd_pal[ *src++ ] << 16;
      *tgt++ = v;
      step -= 2;
      if (step <= 0) { src++; step += step_max; }
    }
    tgt += 160; // fix for retrogame
    src = start_src + HUGO_MAX_WIDTH;
  }
}

static void
loc_put_image_fast()
{
  u32  screen_w = io.screen_w;
  if (screen_w > 320) {
    loc_put_image_stretch_fast();
  } else {
    u8*  src = osd_gfx_buffer;
    u32* tgt = back_surface->pixels;
    u32  screen_h = io.screen_h;

    if (screen_w <= 256) {
      tgt += ( 320 - screen_w ) / 4;
    }
    if (screen_h <= 240) {
      tgt += 160 * (( 240 - screen_h ) / 2);
    } else {
      src += HUGO_MAX_WIDTH * ((screen_h - 240) / 2);
      screen_h = 240;
    }
    while (screen_h-- > 0) {
      u32 w = screen_w / 4;
      u32 v;
      while (w-- > 0) {
        v = osd_pal[ *src++ ];
        v |= osd_pal[ *src++ ] << 16;
        *tgt++ = v;
        v = osd_pal[ *src++ ];
        v |= osd_pal[ *src++ ] << 16;
        *tgt++ = v;
      }
      tgt += 160; // fix for retrogame
      tgt += (320 - screen_w) / 2;
      src += (HUGO_MAX_WIDTH - screen_w);
    }
  }
}

static inline u32
loc_blend_pixel(u32 c0, u32 c1)
{
  return (c0 & c1) + (((c0 ^ c1) & 0xf7def7deU) >> 1);
}

static inline void
loc_blend_15(u16 *dest, u8 *src)
{
  u32 t0, t1, t2;

  t0  = (u16)osd_pal[ src[0] ];
  t1  = (u16)osd_pal[ src[1] ];
  t1 |= (u16)(osd_pal[ src[2] ])<<16;
  t0 |= (u16)(osd_pal[ src[3] ])<<16;
  t2  = loc_blend_pixel(t0, t1);

  dest[0] = t0;
  dest[1] = t2;

  dest[2] = t1;
  dest[3] = t1 >> 16;

  dest[4] = t2 >> 16;
  dest[5] = t0 >> 16;
}

static void
loc_put_image_fast_max()
{
  u32 screen_w = io.screen_w;

  if (screen_w > 320) {
    loc_put_image_stretch_fast();
  } else
  if (screen_w > 256) {
    loc_put_image_fast();
  } else {
    // 256x240
    u8* src  = osd_gfx_buffer;
    u16* tgt = back_surface->pixels;
    u32 screen_h = io.screen_h;

    if (screen_w < 256) {
      tgt += ( 320 - ( screen_w * 125 ) / 100 ) / 2;
    }
    if (screen_h <= 240) {
      tgt += 320 * (( 240 - screen_h ) / 2);
    } else {
      src += HUGO_MAX_WIDTH * ((screen_h - 240) / 2);
      screen_h = 240;
    }
    while (screen_h-- > 0) {
      int w = screen_w / 4;
      u16 *tgt_next = tgt + 320 + 320; // fix for retrogame
      while (w-- > 0) {
        loc_blend_15( tgt, src );
        tgt += 5; src += 4;
      }
      src += (HUGO_MAX_WIDTH - screen_w);
      tgt  = tgt_next;
    }
  }
}

void
psp_hugo_wait_vsync()
{
}

static void
hugo_synchronize()
{
  static u32 nextclock = 1;
  static u32 next_sec_clock = 0;
  static u32 cur_num_frame = 0;

  u32 curclock = SDL_GetTicks();

  if (HUGO.hugo_speed_limiter) {
    while (curclock < nextclock) {
     curclock = SDL_GetTicks();
    }
    u32 f_period = 1000 / HUGO.hugo_speed_limiter;
    nextclock += f_period;
    if (nextclock < curclock) nextclock = curclock + f_period;
  }

  if (HUGO.hugo_view_fps) {
    cur_num_frame++;
    if (curclock > next_sec_clock) {
      next_sec_clock = curclock + 1000;
      HUGO.hugo_current_fps = cur_num_frame * (1 + HUGO.psp_skip_max_frame);
      cur_num_frame = 0;
    }
  }
}

//Graphic rendering 
void
hugo_render_update()
{
# if 0 // defined(LINUX_MODE)
  static u32 screen_w_prev = 0;
  static u32 screen_h_prev = 0;
  if ((screen_w_prev != io.screen_w) ||
      (screen_h_prev != io.screen_h)) {
    screen_w_prev = io.screen_w;
    screen_h_prev = io.screen_h;
    fprintf(stdout, "width=%d height=%d\n", io.screen_w, io.screen_h);
  }
# endif

  if (HUGO.psp_skip_cur_frame <= 0) {

    HUGO.psp_skip_cur_frame = HUGO.psp_skip_max_frame;

    if (HUGO.hugo_render_mode == HUGO_RENDER_FAST    ) loc_put_image_fast();
    else
    if (HUGO.hugo_render_mode == HUGO_RENDER_FAST_MAX) loc_put_image_fast_max();
    else
    if (HUGO.hugo_render_mode == HUGO_RENDER_FIT     ) loc_put_image_fit();

    hugo_synchronize();

    if (psp_kbd_is_danzeff_mode()) {
      danzeff_moveTo(-40, -75);
      danzeff_render( HUGO.danzeff_trans );
    }

    if (HUGO.hugo_view_fps) {
      char buffer[32];
      sprintf(buffer, "%03d %3d", HUGO.hugo_current_clock, (int)HUGO.hugo_current_fps );
      psp_sdl_fill_print(0, 0, buffer, 0xffffff, 0 );
    }

    if (HUGO.hugo_vsync) {
      psp_hugo_wait_vsync();
    }
    psp_sdl_flip();

    if (psp_screenshot_mode) {
      psp_screenshot_mode--;
      if (psp_screenshot_mode <= 0) {
        psp_audio_pause();
        psp_sdl_save_screenshot();
        psp_audio_resume();
        psp_screenshot_mode = 0;
      }
    }

  } else if (HUGO.psp_skip_max_frame) {
    HUGO.psp_skip_cur_frame--;
  }
}

void
hugo_update_save_name(char *Name)
{
  char        TmpFileName[MAX_PATH];
  struct stat aStat;
  int         index;
  char       *SaveName;
  char       *Scan1;
  char       *Scan2;

  SaveName = strrchr(Name,'/');
  if (SaveName != (char *)0) SaveName++;
  else                       SaveName = Name;

  if (!strncasecmp(SaveName, "sav_", 4)) {
    Scan1 = SaveName + 4;
    Scan2 = strrchr(Scan1, '_');
    if (Scan2 && (Scan2[1] >= '0') && (Scan2[1] <= '5')) {
      strncpy(HUGO.hugo_save_name, Scan1, MAX_PATH);
      HUGO.hugo_save_name[Scan2 - Scan1] = '\0';
    } else {
      strncpy(HUGO.hugo_save_name, SaveName, MAX_PATH);
    }
  } else {
    strncpy(HUGO.hugo_save_name, SaveName, MAX_PATH);
  }

  if (HUGO.hugo_save_name[0] == '\0') {
    strcpy(HUGO.hugo_save_name,"default");
  }

  for (index = 0; index < HUGO_MAX_SAVE_STATE; index++) {
    HUGO.hugo_save_state[index].used  = 0;
    memset(&HUGO.hugo_save_state[index].date, 0, sizeof(time_t));
    HUGO.hugo_save_state[index].thumb = 0;

    snprintf(TmpFileName, MAX_PATH, "%s/save/sav_%s_%d.stz", HUGO.hugo_home_dir, HUGO.hugo_save_name, index);
    if (! stat(TmpFileName, &aStat)) {
      HUGO.hugo_save_state[index].used = 1;
      HUGO.hugo_save_state[index].date = aStat.st_mtime;
      snprintf(TmpFileName, MAX_PATH, "%s/save/sav_%s_%d.png", HUGO.hugo_home_dir, HUGO.hugo_save_name, index);
      if (! stat(TmpFileName, &aStat)) {
        if (psp_sdl_load_thumb_png(HUGO.hugo_save_state[index].surface, TmpFileName)) {
          HUGO.hugo_save_state[index].thumb = 1;
        }
      }
    }
  }

  HUGO.comment_present = 0;
  snprintf(TmpFileName, MAX_PATH, "%s/txt/%s.txt", HUGO.hugo_home_dir, HUGO.hugo_save_name);
  if (! stat(TmpFileName, &aStat)) {
    HUGO.comment_present = 1;
  }
}

void
reset_save_name()
{
  hugo_update_save_name("");
}

typedef struct thumb_list {
  struct thumb_list *next;
  char              *name;
  char              *thumb;
} thumb_list;

static thumb_list* loc_head_thumb = 0;

static void
loc_del_thumb_list()
{
  while (loc_head_thumb != 0) {
    thumb_list *del_elem = loc_head_thumb;
    loc_head_thumb = loc_head_thumb->next;
    if (del_elem->name) free( del_elem->name );
    if (del_elem->thumb) free( del_elem->thumb );
    free(del_elem);
  }
}

static void
loc_add_thumb_list(char* filename)
{
  thumb_list *new_elem;
  char tmp_filename[MAX_PATH];

  strcpy(tmp_filename, filename);
  char* save_name = tmp_filename;

  /* .png extention */
  char* Scan = strrchr(save_name, '.');
  if ((! Scan) || (strcasecmp(Scan, ".png"))) return;
  *Scan = 0;

  if (strncasecmp(save_name, "sav_", 4)) return;
  save_name += 4;

  Scan = strrchr(save_name, '_');
  if (! Scan) return;
  *Scan = 0;

  /* only one png for a given save name */
  new_elem = loc_head_thumb;
  while (new_elem != 0) {
    if (! strcasecmp(new_elem->name, save_name)) return;
    new_elem = new_elem->next;
  }

  new_elem = (thumb_list *)malloc( sizeof( thumb_list ) );
  new_elem->next = loc_head_thumb;
  loc_head_thumb = new_elem;
  new_elem->name  = strdup( save_name );
  new_elem->thumb = strdup( filename );
}

void
load_thumb_list()
{
  char SaveDirName[MAX_PATH];
  DIR* fd = 0;

  loc_del_thumb_list();

  snprintf(SaveDirName, MAX_PATH, "%s/save", HUGO.hugo_home_dir);

  fd = opendir(SaveDirName);
  if (!fd) return;

  struct dirent *a_dirent;
  while ((a_dirent = readdir(fd)) != 0) {
    if(a_dirent->d_name[0] == '.') continue;
    if (a_dirent->d_type != DT_DIR) 
    {
      loc_add_thumb_list( a_dirent->d_name );
    }
  }
  closedir(fd);
}

int
load_thumb_if_exists(char *Name)
{
  char        FileName[MAX_PATH];
  char        ThumbFileName[MAX_PATH];
  struct stat aStat;
  char       *SaveName;
  char       *Scan;

  strcpy(FileName, Name);
  SaveName = strrchr(FileName,'/');
  if (SaveName != (char *)0) SaveName++;
  else                       SaveName = FileName;

  Scan = strrchr(SaveName,'.');
  if (Scan) *Scan = '\0';

  if (!SaveName[0]) return 0;

  thumb_list *scan_list = loc_head_thumb;
  while (scan_list != 0) {
    if (! strcasecmp( SaveName, scan_list->name)) {
      snprintf(ThumbFileName, MAX_PATH, "%s/save/%s", HUGO.hugo_home_dir, scan_list->thumb);
      if (! stat(ThumbFileName, &aStat)) {
        if (psp_sdl_load_thumb_png(save_surface, ThumbFileName)) {
          return 1;
        }
      }
    }
    scan_list = scan_list->next;
  }
  return 0;
}

typedef struct comment_list {
  struct comment_list *next;
  char              *name;
  char              *filename;
} comment_list;

static comment_list* loc_head_comment = 0;

static void
loc_del_comment_list()
{
  while (loc_head_comment != 0) {
    comment_list *del_elem = loc_head_comment;
    loc_head_comment = loc_head_comment->next;
    if (del_elem->name) free( del_elem->name );
    if (del_elem->filename) free( del_elem->filename );
    free(del_elem);
  }
}

static void
loc_add_comment_list(char* filename)
{
  comment_list *new_elem;
  char  tmp_filename[MAX_PATH];

  strcpy(tmp_filename, filename);
  char* save_name = tmp_filename;

  /* .png extention */
  char* Scan = strrchr(save_name, '.');
  if ((! Scan) || (strcasecmp(Scan, ".txt"))) return;
  *Scan = 0;

  /* only one txt for a given save name */
  new_elem = loc_head_comment;
  while (new_elem != 0) {
    if (! strcasecmp(new_elem->name, save_name)) return;
    new_elem = new_elem->next;
  }

  new_elem = (comment_list *)malloc( sizeof( comment_list ) );
  new_elem->next = loc_head_comment;
  loc_head_comment = new_elem;
  new_elem->name  = strdup( save_name );
  new_elem->filename = strdup( filename );
}

void
load_comment_list()
{
  char SaveDirName[MAX_PATH];
  DIR* fd = 0;

  loc_del_comment_list();

  snprintf(SaveDirName, MAX_PATH, "%s/txt", HUGO.hugo_home_dir);

  fd = opendir(SaveDirName);
  if (!fd) return;

  struct dirent *a_dirent;
  while ((a_dirent = readdir(fd)) != 0) {
    if(a_dirent->d_name[0] == '.') continue;
    if (a_dirent->d_type != DT_DIR) 
    {
      loc_add_comment_list( a_dirent->d_name );
    }
  }
  closedir(fd);
}

char*
load_comment_if_exists(char *Name)
{
static char loc_comment_buffer[128];

  char        FileName[MAX_PATH];
  char        TmpFileName[MAX_PATH];
  FILE       *a_file;
  char       *SaveName;
  char       *Scan;

  loc_comment_buffer[0] = 0;

  strcpy(FileName, Name);
  SaveName = strrchr(FileName,'/');
  if (SaveName != (char *)0) SaveName++;
  else                       SaveName = FileName;

  Scan = strrchr(SaveName,'.');
  if (Scan) *Scan = '\0';

  if (!SaveName[0]) return 0;

  comment_list *scan_list = loc_head_comment;
  while (scan_list != 0) {
    if (! strcasecmp( SaveName, scan_list->name)) {
      snprintf(TmpFileName, MAX_PATH, "%s/txt/%s", HUGO.hugo_home_dir, scan_list->filename);
      a_file = fopen(TmpFileName, "r");
      if (a_file) {
        char* a_scan = 0;
        loc_comment_buffer[0] = 0;
        if (fgets(loc_comment_buffer, 60, a_file) != 0) {
          a_scan = strchr(loc_comment_buffer, '\n');
          if (a_scan) *a_scan = '\0';
          /* For this #@$% of windows ! */
          a_scan = strchr(loc_comment_buffer,'\r');
          if (a_scan) *a_scan = '\0';
          fclose(a_file);
          return loc_comment_buffer;
        }
        fclose(a_file);
        return 0;
      }
    }
    scan_list = scan_list->next;
  }
  return 0;
}

void
hugo_kbd_load(void)
{
  char        TmpFileName[MAX_PATH + 1];
  struct stat aStat;

  snprintf(TmpFileName, MAX_PATH, "%s/kbd/%s.kbd", HUGO.hugo_home_dir, HUGO.hugo_save_name );
  if (! stat(TmpFileName, &aStat)) {
    psp_kbd_load_mapping(TmpFileName);
  }
}

int
hugo_kbd_save(void)
{
  char TmpFileName[MAX_PATH + 1];
  snprintf(TmpFileName, MAX_PATH, "%s/kbd/%s.kbd", HUGO.hugo_home_dir, HUGO.hugo_save_name );
  return( psp_kbd_save_mapping(TmpFileName) );
}

void
hugo_joy_load(void)
{
  char        TmpFileName[MAX_PATH + 1];
  struct stat aStat;

  snprintf(TmpFileName, MAX_PATH, "%s/joy/%s.joy", HUGO.hugo_home_dir, HUGO.hugo_save_name );
  if (! stat(TmpFileName, &aStat)) {
    psp_joy_load_settings(TmpFileName);
  }
}

int
hugo_joy_save(void)
{
  char TmpFileName[MAX_PATH + 1];
  snprintf(TmpFileName, MAX_PATH, "%s/joy/%s.joy", HUGO.hugo_home_dir, HUGO.hugo_save_name );
  return( psp_joy_save_settings(TmpFileName) );
}

void
hugo_emulator_reset(void)
{
  main_hugo_emulator_reset();
}

void
myPowerSetClockFrequency(int cpu_clock)
{
  if (HUGO.hugo_current_clock != cpu_clock) {
    gp2xPowerSetClockFrequency(cpu_clock);
    HUGO.hugo_current_clock = cpu_clock;
  }
}

void
hugo_default_settings()
{
  HUGO.hugo_snd_enable        = 1;
  HUGO.hugo_snd_volume        = 100;
  HUGO.hugo_snd_freq          = 3; /* stereo 44k */
  HUGO.hugo_render_mode       = HUGO_RENDER_FAST;
  HUGO.hugo_vsync             = 0;
  HUGO.danzeff_trans          = 1;
  HUGO.hugo_overclock         = 16;
  HUGO.hugo_speed_limiter     = 60;
  HUGO.psp_cpu_clock          = GP2X_DEF_EMU_CLOCK;
  HUGO.psp_screenshot_id      = 0;
  HUGO.hugo_view_fps          = 0;

  myPowerSetClockFrequency(HUGO.psp_cpu_clock);
  h6280_set_overclock(HUGO.hugo_overclock);
  osd_psp_set_sound_freq(HUGO.hugo_snd_freq);
}

static int
loc_hugo_save_settings(char *chFileName)
{
  FILE* FileDesc;
  int   error = 0;

  FileDesc = fopen(chFileName, "w");
  if (FileDesc != (FILE *)0 ) {

    fprintf(FileDesc, "psp_cpu_clock=%d\n"      , HUGO.psp_cpu_clock);
    fprintf(FileDesc, "danzeff_trans=%d\n"      , HUGO.danzeff_trans);
    fprintf(FileDesc, "psp_skip_max_frame=%d\n" , HUGO.psp_skip_max_frame);
    fprintf(FileDesc, "hugo_view_fps=%d\n"     , HUGO.hugo_view_fps);
    fprintf(FileDesc, "hugo_snd_enable=%d\n"   , HUGO.hugo_snd_enable);
    fprintf(FileDesc, "hugo_snd_freq=%d\n"   , HUGO.hugo_snd_freq);
    fprintf(FileDesc, "hugo_overclock=%d\n"    , HUGO.hugo_overclock);
    fprintf(FileDesc, "hugo_snd_volume=%d\n"   , HUGO.hugo_snd_volume);
    fprintf(FileDesc, "hugo_render_mode=%d\n"  , HUGO.hugo_render_mode);
    fprintf(FileDesc, "hugo_vsync=%d\n"        , HUGO.hugo_vsync);
    fprintf(FileDesc, "hugo_speed_limiter=%d\n", HUGO.hugo_speed_limiter);

    fclose(FileDesc);

  } else {
    error = 1;
    printf("Failed to open: %s\n", chFileName);
  }

  return error;
}

int
hugo_save_settings(void)
{
  char  FileName[MAX_PATH+1];
  int   error;

  error = 1;

  snprintf(FileName, MAX_PATH, "%s/set/%s.set", HUGO.hugo_home_dir, HUGO.hugo_save_name);
  error = loc_hugo_save_settings(FileName);

  return error;
}

static int
loc_hugo_load_settings(char *chFileName)
{
  char  Buffer[512];
  char *Scan;
  unsigned int Value;
  FILE* FileDesc;

  FileDesc = fopen(chFileName, "r");
  if (FileDesc == (FILE *)0 ) return 0;

  while (fgets(Buffer,512, FileDesc) != (char *)0) {

    Scan = strchr(Buffer,'\n');
    if (Scan) *Scan = '\0';
    /* For this #@$% of windows ! */
    Scan = strchr(Buffer,'\r');
    if (Scan) *Scan = '\0';
    if (Buffer[0] == '#') continue;

    Scan = strchr(Buffer,'=');
    if (! Scan) continue;

    *Scan = '\0';
    Value = atoi(Scan+1);

    if (!strcasecmp(Buffer,"psp_cpu_clock"))      HUGO.psp_cpu_clock = Value;
    else
    if (!strcasecmp(Buffer,"hugo_view_fps"))     HUGO.hugo_view_fps = Value;
    else
    if (!strcasecmp(Buffer,"danzeff_trans"))      HUGO.danzeff_trans = Value;
    else
    if (!strcasecmp(Buffer,"psp_skip_max_frame")) HUGO.psp_skip_max_frame = Value;
    else
    if (!strcasecmp(Buffer,"hugo_snd_enable"))   HUGO.hugo_snd_enable = Value;
    else
    if (!strcasecmp(Buffer,"hugo_snd_freq"))   HUGO.hugo_snd_freq = Value;
    else
    if (!strcasecmp(Buffer,"hugo_overclock"))   HUGO.hugo_overclock = Value;
    else
    if (!strcasecmp(Buffer,"hugo_snd_volume"))   HUGO.hugo_snd_volume = Value;
    else
    if (!strcasecmp(Buffer,"hugo_render_mode"))  HUGO.hugo_render_mode = Value;
    else
    if (!strcasecmp(Buffer,"hugo_vsync"))  HUGO.hugo_vsync = Value;
    else
    if (!strcasecmp(Buffer,"hugo_speed_limiter"))  HUGO.hugo_speed_limiter = Value;
  }

  fclose(FileDesc);

  myPowerSetClockFrequency(HUGO.psp_cpu_clock);
  h6280_set_overclock(HUGO.hugo_overclock);
  osd_psp_set_sound_freq(HUGO.hugo_snd_freq);
  osd_psp_set_sound_volume(HUGO.hugo_snd_volume);

  return 0;
}

int
hugo_load_settings()
{
  char  FileName[MAX_PATH+1];
  int   error;

  error = 1;

  snprintf(FileName, MAX_PATH, "%s/set/%s.set", HUGO.hugo_home_dir, HUGO.hugo_save_name);
  error = loc_hugo_load_settings(FileName);

  return error;
}

int
hugo_load_file_settings(char *FileName)
{
  return loc_hugo_load_settings(FileName);
}

static int
loc_hugo_save_cheat(char *chFileName)
{
  FILE* FileDesc;
  int   cheat_num;
  int   error = 0;

  FileDesc = fopen(chFileName, "w");
  if (FileDesc != (FILE *)0 ) {

    for (cheat_num = 0; cheat_num < HUGO_MAX_CHEAT; cheat_num++) {
      Hugo_cheat_t* a_cheat = &HUGO.hugo_cheat[cheat_num];
      if (a_cheat->type != HUGO_CHEAT_NONE) {
        fprintf(FileDesc, "%d,%x,%x,%s\n", 
                a_cheat->type, a_cheat->addr, a_cheat->value, a_cheat->comment);
      }
    }
    fclose(FileDesc);

  } else {
    error = 1;
  }

  return error;
}

int
hugo_save_cheat(void)
{
  char  FileName[MAX_PATH+1];
  int   error;

  error = 1;

  snprintf(FileName, MAX_PATH, "%s/cht/%s.cht", HUGO.hugo_home_dir, HUGO.hugo_save_name);
  error = loc_hugo_save_cheat(FileName);

  return error;
}

static int
loc_hugo_load_cheat(char *chFileName)
{
  char  Buffer[512];
  char *Scan;
  char *Field;
  unsigned int  cheat_addr;
  unsigned int  cheat_value;
  unsigned int  cheat_type;
  char         *cheat_comment;
  int           cheat_num;
  FILE* FileDesc;

  memset(HUGO.hugo_cheat, 0, sizeof(HUGO.hugo_cheat));
  cheat_num = 0;

  FileDesc = fopen(chFileName, "r");
  if (FileDesc == (FILE *)0 ) return 0;

  while (fgets(Buffer,512, FileDesc) != (char *)0) {

    Scan = strchr(Buffer,'\n');
    if (Scan) *Scan = '\0';
    /* For this #@$% of windows ! */
    Scan = strchr(Buffer,'\r');
    if (Scan) *Scan = '\0';
    if (Buffer[0] == '#') continue;

    /* %d, %x, %x, %s */
    Field = Buffer;
    Scan = strchr(Field, ',');
    if (! Scan) continue;
    *Scan = 0;
    if (sscanf(Field, "%d", &cheat_type) != 1) continue;
    Field = Scan + 1;
    Scan = strchr(Field, ',');
    if (! Scan) continue;
    *Scan = 0;
    if (sscanf(Field, "%x", &cheat_addr) != 1) continue;
    Field = Scan + 1;
    Scan = strchr(Field, ',');
    if (! Scan) continue;
    *Scan = 0;
    if (sscanf(Field, "%x", &cheat_value) != 1) continue;
    Field = Scan + 1;
    cheat_comment = Field;

    if (cheat_type <= HUGO_CHEAT_NONE) continue;

    Hugo_cheat_t* a_cheat = &HUGO.hugo_cheat[cheat_num];

    a_cheat->type  = cheat_type;
    a_cheat->addr  = cheat_addr;
    a_cheat->value = cheat_value;
    strncpy(a_cheat->comment, cheat_comment, sizeof(a_cheat->comment));
    a_cheat->comment[sizeof(a_cheat->comment)-1] = 0;

    if (++cheat_num >= HUGO_MAX_CHEAT) break;
  }
  fclose(FileDesc);

  return 0;
}

int
hugo_load_cheat()
{
  char  FileName[MAX_PATH+1];
  int   error;

  error = 1;

  snprintf(FileName, MAX_PATH, "%s/cht/%s.cht", HUGO.hugo_home_dir, HUGO.hugo_save_name);
  error = loc_hugo_load_cheat(FileName);

  return error;
}

int
hugo_load_file_cheat(char *FileName)
{
  return loc_hugo_load_cheat(FileName);
}

void
hugo_apply_cheats()
{
  int cheat_num;
  for (cheat_num = 0; cheat_num < HUGO_MAX_CHEAT; cheat_num++) {
    Hugo_cheat_t* a_cheat = &HUGO.hugo_cheat[cheat_num];
    if (a_cheat->type == HUGO_CHEAT_ENABLE) {
      writeindexedram(a_cheat->addr, a_cheat->value);
    }
  }
}

void
hugo_audio_pause(void)
{
  if (HUGO.hugo_snd_enable) {
    SDL_PauseAudio(1);
  }
}

void
hugo_audio_resume(void)
{
  if (HUGO.hugo_snd_enable) {
    SDL_PauseAudio(0);
  }
}

int 
loc_hugo_save_state(char *filename)
{
  return main_hugo_save_state(filename);
}

int
hugo_load_rom(char *FileName, int zip_format)
{
  char   SaveName[MAX_PATH+1];
  char*  ExtractName;
  char*  scan;
  int    error;
  size_t unzipped_size;

  error = 1;

  if (zip_format) {

    ExtractName = find_possible_filename_in_zip( FileName, "pce.bin");
    if (ExtractName) {
      strncpy(SaveName, FileName, MAX_PATH);
      scan = strrchr(SaveName,'.');
      if (scan) *scan = '\0';
      hugo_update_save_name(SaveName);
      const char* rom_buffer = (const char *)extract_file_in_memory ( FileName, ExtractName, &unzipped_size);
      if (rom_buffer) {
        error = ! main_hugo_load_rom_buffer( rom_buffer, unzipped_size );
      }
    }

  } else {
    strncpy(SaveName,FileName,MAX_PATH);
    scan = strrchr(SaveName,'.');
    if (scan) *scan = '\0';
    hugo_update_save_name(SaveName);
    error = ! main_hugo_load_rom(FileName);
  }

  if (! error ) {
    hugo_emulator_reset();
    hugo_kbd_load();
    hugo_joy_load();
    hugo_load_cheat();
    hugo_load_settings();
  }

  return error;
}

int
hugo_load_cd(char *FileName)
{
  char   SaveName[MAX_PATH+1];
  int    error;
  char  *scan;

  error = 1;

  strncpy(SaveName,FileName,MAX_PATH);
  scan = strrchr(SaveName,'.');
  if (scan) *scan = '\0';
  hugo_update_save_name(SaveName);
  error = ! main_hugo_load_cd(FileName);

  if (! error ) {
    hugo_emulator_reset();
    hugo_kbd_load();
    hugo_joy_load();
    hugo_load_cheat();
    hugo_load_settings();
  }

  return error;
}

int
hugo_snapshot_save_slot(int save_id)
{
  char      FileName[MAX_PATH+1];
  struct stat aStat;
  int       error;

  error = 1;

  if (save_id < HUGO_MAX_SAVE_STATE) {
    snprintf(FileName, MAX_PATH, "%s/save/sav_%s_%d.stz", HUGO.hugo_home_dir, HUGO.hugo_save_name, save_id);
    error = loc_hugo_save_state(FileName);
    if (! error) {
      if (! stat(FileName, &aStat)) {
        HUGO.hugo_save_state[save_id].used  = 1;
        HUGO.hugo_save_state[save_id].thumb = 0;
        HUGO.hugo_save_state[save_id].date  = aStat.st_mtime;
        snprintf(FileName, MAX_PATH, "%s/save/sav_%s_%d.png", HUGO.hugo_home_dir, HUGO.hugo_save_name, save_id);
        if (psp_sdl_save_thumb_png(HUGO.hugo_save_state[save_id].surface, FileName)) {
          HUGO.hugo_save_state[save_id].thumb = 1;
        }
      }
    }
  }

  return error;
}

int
hugo_snapshot_load_slot(int load_id)
{
  char  FileName[MAX_PATH+1];
  int   error;

  error = 1;

  if (load_id < HUGO_MAX_SAVE_STATE) {
    snprintf(FileName, MAX_PATH, "%s/save/sav_%s_%d.stz", HUGO.hugo_home_dir, HUGO.hugo_save_name, load_id);
    error = main_hugo_load_state(FileName);
  }
  return error;
}

int
hugo_snapshot_del_slot(int save_id)
{
  char  FileName[MAX_PATH+1];
  struct stat aStat;
  int   error;

  error = 1;

  if (save_id < HUGO_MAX_SAVE_STATE) {
    snprintf(FileName, MAX_PATH, "%s/save/sav_%s_%d.stz", HUGO.hugo_home_dir, HUGO.hugo_save_name, save_id);
    error = remove(FileName);
    if (! error) {
      HUGO.hugo_save_state[save_id].used  = 0;
      HUGO.hugo_save_state[save_id].thumb = 0;
      memset(&HUGO.hugo_save_state[save_id].date, 0, sizeof(time_t));

      /* We keep always thumbnail with id 0, to have something to display in the file requester */ 
      if (save_id != 0) {
        snprintf(FileName, MAX_PATH, "%s/save/sav_%s_%d.png", HUGO.hugo_home_dir, HUGO.hugo_save_name, save_id);
        if (! stat(FileName, &aStat)) {
          remove(FileName);
        }
      }
    }
  }

  return error;
}

void
hugo_treat_command_key(int hugo_idx)
{
  int new_render;

  psp_audio_pause();

  switch (hugo_idx) 
  {
    case HUGOC_FPS: HUGO.hugo_view_fps = ! HUGO.hugo_view_fps;
    break;
    case HUGOC_JOY: HUGO.psp_reverse_analog = ! HUGO.psp_reverse_analog;
    break;
    case HUGOC_RENDER: 
      psp_sdl_black_screen();
      new_render = HUGO.hugo_render_mode + 1;
      if (new_render > HUGO_LAST_RENDER) new_render = 0;
      HUGO.hugo_render_mode = new_render;
    break;
    case HUGOC_LOAD: 
       psp_main_menu_load_current();
    break;
    case HUGOC_SAVE: 
       psp_main_menu_save_current(); 
    break;
    case HUGOC_RESET: 
       psp_sdl_black_screen();
       main_hugo_emulator_reset(); 
       reset_save_name();
    break;
    case HUGOC_AUTOFIRE: 
       kbd_change_auto_fire(! HUGO.hugo_auto_fire);
    break;
    case HUGOC_AUTOFIREB: 
       kbd_change_auto_fire_button(! HUGO.hugo_auto_fire_button);
    break;
    case HUGOC_DECFIRE: 
      if (HUGO.hugo_auto_fire_period > 0) HUGO.hugo_auto_fire_period--;
    break;
    case HUGOC_INCFIRE: 
      if (HUGO.hugo_auto_fire_period < 19) HUGO.hugo_auto_fire_period++;
    break;
    case HUGOC_SCREEN: psp_screenshot_mode = 10;
    break;
  }
  psp_audio_resume();
}

void
psp_global_initialize()
{
  memset(&HUGO, 0, sizeof(Hugo_t));
  strcpy(HUGO.hugo_home_dir,"/mnt/game/hugo");
  hugo_default_settings();
  psp_joy_default_settings();
  psp_kbd_default_settings();

  psp_sdl_init();

  hugo_update_save_name("");
  hugo_load_settings();
  hugo_kbd_load();
  hugo_joy_load();
  hugo_load_cheat();

  myPowerSetClockFrequency(HUGO.psp_cpu_clock);
}

extern int main_hugo(int argc, char* argv[]);

int
SDL_main(int argc,char **argv)
{
  psp_global_initialize();

  main_hugo(argc, argv);

  return(0);
}
