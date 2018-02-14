#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include "font.h"

#define TITLE_MSG "RetroGame(RS-97) Overclock"

#define BASE      0x10000000
#define CPCCR     (0x00 >> 2)
#define CPPCR     (0x10 >> 2)
#define CPPSR     (0x14 >> 2)
#define CPPCR1    (0x30 >> 2)
#define CPSPR     (0x34 >> 2)
#define CPSPPR    (0x38 >> 2)
#define USBPCR    (0x3C >> 2)
#define USBRDT    (0x40 >> 2)
#define USBVBFIL  (0x44 >> 2)
#define USBCDR    (0x50 >> 2)
#define I2SCDR    (0x60 >> 2)
#define LPCDR     (0x64 >> 2)
#define MSCCDR    (0x68 >> 2)
#define UHCCDR    (0x6C >> 2)
#define SSICDR    (0x74 >> 2)
#define CIMCDR    (0x7C >> 2)
#define GPSCDR    (0x80 >> 2)
#define PCMCDR    (0x84 >> 2)
#define GPUCDR    (0x88 >> 2)

int fd=-1;
SDL_RWops *rw;
TTF_Font *font=NULL;
SDL_Surface *ScreenSurface=NULL;
SDL_Surface *screen=NULL;
extern uint8_t rwfont[];
volatile unsigned long *mem;

void draw_background(void)
{
  SDL_Rect rt;

  // msg box
  rt.x = 40;
  rt.y = 40;
  rt.w = 320 - 50;
  rt.h = 240 - 50;
  SDL_FillRect(screen, &rt, SDL_MapRGB(screen->format, 0, 0, 0));
  rt.x = 30;
  rt.y = 30;
  rt.w = 320 - 50;
  rt.h = 240 - 50;
  SDL_FillRect(screen, &rt, SDL_MapRGB(screen->format, 0xa0, 0xa0, 0xa0));
}

void draw_msg(int m)
{
  char buf[64];
  SDL_Rect rt={0}; 
  SDL_Color msg_color = {0, 0, 0};
  SDL_Color title_color = {255, 0, 0};
  SDL_Color info_color = {128, 0, 64};

  // background
  draw_background();

  // title
  SDL_Surface *msg = TTF_RenderText_Solid(font, TITLE_MSG, title_color);
  rt.x = 70;
  rt.y = 8;
  SDL_BlitSurface(msg, NULL, screen, &rt);
  SDL_FreeSurface(msg);

  // current clock
  rt.x = 45;
  rt.y = 35;
#if !defined(DEBUG)
  sprintf(buf, "Cur clock: %dMHz (M=%d)", ((mem[CPPCR] >> 24) & 0xff)*6, ((mem[CPPCR] >> 24) & 0xff));
#else
  sprintf(buf, "Cur clock: 528MHz (M=88)");
#endif
  msg = TTF_RenderText_Solid(font, buf, msg_color);
  SDL_BlitSurface(msg, NULL, screen, &rt);
  SDL_FreeSurface(msg);
  
	rt.x = 45;
  rt.y = 55;
  sprintf(buf, "New clock: %dMHz (M=%d)", m*6, m);
  msg = TTF_RenderText_Solid(font, buf, msg_color);
  SDL_BlitSurface(msg, NULL, screen, &rt);
  SDL_FreeSurface(msg);
	
  rt.x = 45;
  rt.y = 100;
  msg = TTF_RenderText_Solid(font, "Clock = (24MHz * M / 2) >> 1, M>=4", info_color);
  SDL_BlitSurface(msg, NULL, screen, &rt);
  SDL_FreeSurface(msg);
  
  rt.x = 45;
  rt.y = 115;
  msg = TTF_RenderText_Solid(font, "Press A: +1", info_color);
  SDL_BlitSurface(msg, NULL, screen, &rt);
  SDL_FreeSurface(msg);
  
  rt.x = 45;
  rt.y = 130;
  msg = TTF_RenderText_Solid(font, "Press B: -1", info_color);
  SDL_BlitSurface(msg, NULL, screen, &rt);
  SDL_FreeSurface(msg);
  
  rt.x = 45;
  rt.y = 145;
  msg = TTF_RenderText_Solid(font, "Press X: +10", info_color);
  SDL_BlitSurface(msg, NULL, screen, &rt);
  SDL_FreeSurface(msg);
  
  rt.x = 45;
  rt.y = 160;
  msg = TTF_RenderText_Solid(font, "Press Y: -10", info_color);
  SDL_BlitSurface(msg, NULL, screen, &rt);
  SDL_FreeSurface(msg);
  
  rt.x = 45;
  rt.y = 175;
  msg = TTF_RenderText_Solid(font, "Press Start: Apply", info_color);
  SDL_BlitSurface(msg, NULL, screen, &rt);
  SDL_FreeSurface(msg);
  
  rt.x = 45;
  rt.y = 190;
  msg = TTF_RenderText_Solid(font, "Press Select: Exit", info_color);
  SDL_BlitSurface(msg, NULL, screen, &rt);
  SDL_FreeSurface(msg);
  
  
	int x, y;
  uint32_t *s = screen->pixels;
  uint32_t *d = ScreenSurface->pixels;
  for(y=0; y<240; y++){
    for(x=0; x<160; x++){
      *d++ = *s++;
    }
    d+= 160;
  } 
  SDL_Flip(ScreenSurface);
}

int main(int argc, char* argv[])
{
  int m=88, loop=1;
  SDL_Event event;

#if !defined(DEBUG)
  fd=open("/dev/mem", O_RDWR);
  if(fd < 0){
    printf("failed to open /dev/mem\n");
    return -1;
  }
  mem  = mmap(0, 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, BASE);
  m = ((mem[CPPCR] >> 24) & 0xff);
#endif

  if(SDL_Init(SDL_INIT_VIDEO) != 0){
    printf("%s, failed to SDL_Init\n", __func__);
    return -1;
  }
  SDL_ShowCursor(0);
 
  ScreenSurface = SDL_SetVideoMode(320, 480, 16, SDL_HWSURFACE);
  screen = SDL_CreateRGBSurface(SDL_HWSURFACE, 320, 240, 16, 0, 0, 0, 0);
  if(screen == NULL){
    printf("%s, failed to SDL_SetVideMode\n", __func__);
    return -1;
  }
  if(TTF_Init() == -1){
    printf("failed to TTF_Init\n");
    return -1;
  }
  rw = SDL_RWFromMem(rwfont, sizeof(rwfont));
  font = TTF_OpenFontRW(rw, 1, 14);
	TTF_SetFontHinting(font, TTF_HINTING_MONO);
	TTF_SetFontOutline(font, 0);
  
  // background
  SDL_FillRect(screen, &screen->clip_rect, SDL_MapRGB(screen->format, 0x00, 0x00, 0xc0));
  draw_msg(m);
	while(loop){
    while(SDL_PollEvent(&event)){
      if(event.type == SDL_KEYDOWN){
				//printf("%d\n", event.key.keysym.sym);
        if(event.key.keysym.sym == 27){ // SDL_q
          loop = 0;
          break;
        }
        if(event.key.keysym.sym == 13){ // SDL_w
#if !defined(DEBUG)
          mem[CPPCR] = (m << 24) | 0x090520;  
          draw_msg(m);
#endif
        }
        if(event.key.keysym.sym == 304){ // 97
          m-= 10;
          if(m < 4){
            m = 4;
          }
          draw_msg(m);
        }
        if(event.key.keysym.sym == 32){ // 115
          m+= 10;
          if(m >= 200){
            m = 200;
          }
          draw_msg(m);
        }
        if(event.key.keysym.sym == 308){ // 122
          m-= 1;
          if(m < 4){
            m = 4;
          }
          draw_msg(m);
        }
        if(event.key.keysym.sym == 306){ // 120
          m+= 1;
          if(m >= 200){
            m = 200;
          }
          draw_msg(m);
        }
      }   
    }   
  }

#if !defined(DEBUG)
	close(fd);
#endif
  SDL_RWclose(rw);
  SDL_Quit();
  return 0;    
}

