#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include <SDL_image.h>
 
int main(int argc, char* args[])
{
  if(SDL_Init(SDL_INIT_VIDEO) != 0){
    printf("%s, failed to SDL_Init\n", __func__);
    return -1;
  }
  SDL_ShowCursor(0);
 
  SDL_Surface* screen;
  screen = SDL_SetVideoMode(320, 480, 16, SDL_HWSURFACE);
  if(screen == NULL){
    printf("%s, failed to SDL_SetVideMode\n", __func__);
    return -1;
  }
 
  SDL_Surface* png = IMG_Load("/usr/share/splash/od.jpg");
  if(png == NULL){
    printf("%s, failed to IMG_Load\n", __func__);
    return -1;
  }

  SDL_BlitSurface(png, NULL, screen, NULL);
  SDL_Flip(screen);
  SDL_Delay(1500);
  SDL_FreeSurface(png);
  SDL_Quit();
  return 0;    
}

