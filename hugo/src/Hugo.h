#ifndef _HUGO_H_
#define _HUGO_H_

# ifdef __cplusplus
extern "C" {
# endif

//LUDO:
# define HUGO_RENDER_FAST     0
# define HUGO_RENDER_FAST_MAX 1
# define HUGO_RENDER_FIT      2
# define HUGO_LAST_RENDER     2

# define MAX_PATH             256
# define HUGO_MAX_SAVE_STATE    5
# define HUGO_MAX_CHEAT        10

#include <SDL.h>

#define HUGO_CHEAT_NONE    0
#define HUGO_CHEAT_ENABLE  1
#define HUGO_CHEAT_DISABLE 2

#define HUGO_CHEAT_COMMENT_SIZE 25

  typedef struct Hugo_cheat_t {
    unsigned char  type;
    unsigned short addr;
    unsigned char  value;
    char           comment[HUGO_CHEAT_COMMENT_SIZE];
  } Hugo_cheat_t;

  typedef struct Hugo_save_t {

    SDL_Surface    *surface;
    char            used;
    char            thumb;
    time_t          date;

  } Hugo_save_t;

  typedef struct Hugo_t {
 
    Hugo_save_t  hugo_save_state[HUGO_MAX_SAVE_STATE];
    Hugo_cheat_t hugo_cheat[HUGO_MAX_CHEAT];

    char hugo_save_name[MAX_PATH];
    char hugo_home_dir[MAX_PATH];

    char comment_present;
    int  psp_screenshot_id;
    int  psp_cpu_clock;
    char psp_reverse_analog;
    char hugo_view_fps;
    int  hugo_current_fps;
    int  hugo_current_clock;
    char psp_active_joystick;
    char hugo_snd_enable;
    int  hugo_snd_volume;
    int  hugo_snd_freq;
    int  hugo_overclock;
    int  hugo_render_mode;
    char hugo_vsync;
    int  danzeff_trans;
    int  hugo_speed_limiter;
    int  psp_skip_max_frame;
    int  psp_skip_cur_frame;
    int  hugo_slow_down_max;
    char hugo_auto_fire;
    char hugo_auto_fire_pressed;
    char hugo_auto_fire_button;
    int  hugo_auto_fire_period;

  } Hugo_t;

  extern Hugo_t HUGO;

  extern void main_hugo_send_key_event(int joy_num, int hugo_idx, int key_press);
  extern int  main_hugo_load_state(char *filename);
  extern void main_hugo_force_draw_blit();
  extern int  main_hugo_save_state(char *filename);
  extern int main_hugo_load_rom(char *filename);
  extern void main_hugo_emulator_reset();

  extern void myPowerSetClockFrequency(int cpu_clock);
  extern int psp_exit_now;

# ifdef __cplusplus
}
# endif

#endif
