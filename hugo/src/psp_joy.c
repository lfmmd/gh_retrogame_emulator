/*
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
#include <math.h>

#include "Hugo.h"
#include "psp_joy.h"
#include "psp_kbd.h"

int
psp_joy_load_settings(char *joy_filename)
{
  FILE    *JoyFile;
  int      error = 0;
  
  JoyFile = fopen(joy_filename, "r");
  error   = 1;

  if (JoyFile != (FILE*)0) {
    psp_joy_load_settings_file(JoyFile);
    error = 0;
    fclose(JoyFile);
  }

  return error;
}

void
psp_joy_default_settings()
{
  HUGO.hugo_auto_fire         = 0;
  HUGO.hugo_auto_fire_period  = 10;
  HUGO.hugo_auto_fire_pressed = 0;
  HUGO.hugo_auto_fire_button  = 1;
  HUGO.psp_reverse_analog     = 0;
  HUGO.psp_active_joystick    = 0;
}

int
psp_joy_load_settings_file(FILE *JoyFile)
{
  char     Buffer[512];
  char    *Scan;
  int      Value = 0;
  int      joy_id = 0;

  while (fgets(Buffer,512,JoyFile) != (char *)0) {
      
    Scan = strchr(Buffer,'\n');
    if (Scan) *Scan = '\0';
    /* For this #@$% of windows ! */
    Scan = strchr(Buffer,'\r');
    if (Scan) *Scan = '\0';
    if (Buffer[0] == '#') continue;

    Scan = strchr(Buffer,'=');
    if (! Scan) continue;
    
    *Scan = '\0';
    Value = atoi(Scan + 1);

    if (!strcasecmp(Buffer,"psp_reverse_analog")) HUGO.psp_reverse_analog = Value;
    else
    if (!strcasecmp(Buffer,"hugo_auto_fire_period")) HUGO.hugo_auto_fire_period = Value;
    else
    if (!strcasecmp(Buffer,"hugo_auto_fire_button")) HUGO.hugo_auto_fire_button = Value;
  }

  return 0;
}

int
psp_joy_save_settings(char *joy_filename)
{
  FILE    *JoyFile;
  int      joy_id = 0;
  int      error = 0;

  JoyFile = fopen(joy_filename, "w");
  error   = 1;

  if (JoyFile != (FILE*)0) {

    fprintf( JoyFile, "psp_reverse_analog=%d\n"   , HUGO.psp_reverse_analog);
    fprintf( JoyFile, "hugo_auto_fire_period=%d\n", HUGO.hugo_auto_fire_period);
    fprintf( JoyFile, "hugo_auto_fire_button=%d\n"   , HUGO.hugo_auto_fire_button);

    error = 0;
    fclose(JoyFile);
  }

  return error;
}

