#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include "font.h"

#define MAX_LINE 11
#define FONT_SIZE 14
#define LINE_INTERVAL 15
#define MOUNT_POINT "/mnt/tmp/"
#define TMP_ROOTFS "/mnt/tmp/rootfs.tar.gz"
#define TMP_GAME "/mnt/tmp/game/"
#define ORG_GAME "/mnt/game/"
#define VERSION "/usr/share/cfw/version"
#define TITLE_MSG "RetroGame(RS-97) CFW v1.0 r20180213"
#define TARGET_PACKAGE "/mnt/int_sd/upgrade.ext3"
#define TARGET_SHA1 "ee47d7bd1ba5e455da5e12eaf8e23b40b770d2d0"

SDL_RWops *rw;
TTF_Font *font = NULL;
SDL_Surface *ScreenSurface = NULL;
SDL_Surface *screen = NULL;
char log[MAX_LINE][64] = {0};
extern uint8_t rwfont[];

void main_exit(int code)
{
  SDL_Delay(5000);
  SDL_RWclose(rw);
  SDL_Quit();
  exit(code);
}

void draw_background(void)
{
  SDL_Rect rt;

  // log box
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
  
  // progress box
  rt.x = 45;
  rt.y = 202;
  rt.w = 320 - 85;
  rt.h = 13;
  SDL_FillRect(screen, &rt, SDL_MapRGB(screen->format, 0x00, 0x00, 0xc0));
}

void draw_progress(int val)
{
  SDL_Rect rt;

  if(val > 100) {
    val = 100;
  }
  if(val < 0){
    val = 0;
  }

  // msg box
  rt.x = 45;
  rt.y = 202;
  rt.w = 35 + (val * 2);
  rt.h = 13;
  SDL_FillRect(screen, &rt, SDL_MapRGB(screen->format, 0xcc, 0x00, 0x00));
}

void draw_log(int percent, char *str)
{
  int i;
  static int index = -1;
  SDL_Rect rt={0}; 
  SDL_Color log_color = {0, 0, 0};
  SDL_Color title_color = {255, 0, 0};

  if (index >= (MAX_LINE-1)) {
    for(i=0; i<MAX_LINE-1; i++){
      strcpy(log[i], log[i+1]);
    }
  }
  else {
    index += 1;
  }
  strcpy(log[index], str);
	printf("info: %s\n", str);

  // background
  draw_background();

  // title
  SDL_Surface *msg = TTF_RenderText_Solid(font, TITLE_MSG, title_color);
  rt.x = 32;
  rt.y = 8;
  SDL_BlitSurface(msg, NULL, screen, &rt);
  SDL_FreeSurface(msg);

  // log
  for (i=0; i<MAX_LINE; i++) {
    SDL_Surface *msg = TTF_RenderText_Solid(font, log[i], log_color);
    rt.x = 45;
    rt.y = 35 + (i * LINE_INTERVAL);
    SDL_BlitSurface(msg, NULL, screen, &rt);
    SDL_FreeSurface(msg);
  }
  draw_progress(percent);
  
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

int check_os(void)
{
  draw_log(0, "check target system...");
  SDL_Delay(1000);

  FILE *fp = fopen("/gmenu2x/gmenu2x.conf", "r");
  if (fp == NULL) {
    fp = fopen("/usr/share/cfw/version", "r");
    if (fp == NULL) {
      draw_log(0, "your system is original fw");
      draw_log(0, "please install cfw os firstly");
      draw_log(0, "Oops !");
      SDL_Delay(5000);
      main_exit(-1);
    }
  	draw_log(10, "cfw os"); 
    fclose(fp);
    return 0;
  }
  draw_log(10, "beta os v20180208"); 
  fclose(fp);
  return 0;
}

int check_sha1(char* file)
{
  int total_size=0, read_size=0;
  char *buffer;
  char buf[64];
  FILE *fp = fopen(file , "r");
  if(fp == NULL){
    return -1;
  }
  draw_log(15, "calculate package sha1...");
  fseek(fp, 0, SEEK_END);
  total_size = ftell(fp);
  rewind(fp);
  buffer = (char*)malloc(sizeof(char)*(total_size + 1));
  read_size = fread(buffer, sizeof(char), total_size, fp);
  if (total_size != read_size) {
    free(buffer);
    fclose(fp);

    sprintf(buf, "failed to open: %s", file);
    draw_log(15, buf);
    draw_log(15, "Oops !");
    main_exit(-1);
  }

  int offset;
  char result[21];
  char hexresult[41];
  SHA1(result, buffer, total_size);
  for( offset = 0; offset < 20; offset++) {
    sprintf( ( hexresult + (2*offset)), "%02x", result[offset]&0xff);
  }
  sprintf(buf, "%.16s...", hexresult);
  printf("sha1: %s\n", hexresult);

  draw_log(20, buf);
  draw_log(25, "verify sha1...");
  if(strcmp(TARGET_SHA1, hexresult) == 0){
    draw_log(25, "matched");
  }
  else{
    free(buffer);
    fclose(fp);

    draw_log(25, "corrupted package");
    draw_log(25, "Oops !");
    main_exit(-1);
  }
  free(buffer);
  fclose(fp);
}

void umount_package(void)
{
  return;

  draw_log(45, "umount upgrade package...");
  SDL_Delay(1000);
  system("umount -f "MOUNT_POINT);
  system("sync");
  system("sync");
  system("sync");
  draw_log(50, "done");
  draw_log(50, "remove mount point...");
  system("rm -rf "MOUNT_POINT);
  draw_log(55, "done");
}

void mount_package(char *file)
{
  char buf[64];

  draw_log(30, "create mount point "MOUNT_POINT"...");
  system("mount -o remount,rw / /");
  system("mkdir -p "MOUNT_POINT);
  draw_log(35, "done");
  draw_log(35, "mount upgrade package...");
  sprintf(buf, "mount %s %s", TARGET_PACKAGE, MOUNT_POINT);
  system(buf);
	SDL_Delay(1000);

  draw_log(40, "check mounted package...");
  FILE *fp = fopen(TMP_ROOTFS, "r");
  if (fp == NULL) {
    draw_log(40, "mount failed !");
    umount_package();
    draw_log(40, "Oops !");
    main_exit(-1);
  }
  draw_log(45, "done");
  fclose(fp);
}

void extract_rootfs(void)
{
	draw_log(80, "extract rootfs...");
	system("cd /;tar xvfp "TMP_ROOTFS);
	draw_log(80, "done"); 
	draw_log(85, "check rootfs...");
	SDL_Delay(1000);

	char ret[64];
	char buf[64];
  FILE *fp = fopen(VERSION, "r");
  if (fp == NULL) {
    draw_log(85, "check failed !");
    umount_package();
    draw_log(85, "Oops !");
    main_exit(-1);
  }
	fread(ret, 1, sizeof(ret), fp);
	sprintf(buf, "ver: %s", ret);
	draw_log(90, buf);
  draw_log(90, "done");
  fclose(fp);
}

void copy_game(void)
{
	draw_log(90, "copy game...");
	SDL_Delay(1000);
	system("cp -ap "TMP_GAME "* "ORG_GAME);
	draw_log(95, "done");
}

int main(int argc, char* argv[])
{
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
  font = TTF_OpenFontRW(rw, 1, FONT_SIZE);
	TTF_SetFontHinting(font, TTF_HINTING_MONO);
	TTF_SetFontOutline(font, 0);
  
  // background
  SDL_FillRect(screen, &screen->clip_rect, SDL_MapRGB(screen->format, 0x00, 0x00, 0xc0));

  check_os();
  check_sha1(TARGET_PACKAGE);
  mount_package(TARGET_PACKAGE);
	extract_rootfs();
	copy_game();
  umount_package();

  system("rm -rf /gmenu2x");
	draw_log(98, "enojy !");
	draw_log(100, "reboot...");
	SDL_Delay(1000);
	system("reboot");
  SDL_RWclose(rw);
  SDL_Quit();
  return 0;    
}

