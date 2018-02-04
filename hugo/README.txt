
    Welcome to PCE Engine Emulator

Original Authors of Hu-go

  See AUTHORS.txt file

Author of the Dingoo and Dingux port version 

  Ludovic Jacomme alias Zx-81 zx81.zx81(at)gmail.com

  Homepage: http://zx81.zx81.free.fr


1. INTRODUCTION
   ------------

  Hu-Go emulates the NEC PC Engine console on many system such as
  Linux and Windows. (see http://www.zeograd.com/ for details)

  Dingux-HUGO is a port on Dingoo/Dingux of my latest Dingoo HUGO 
  port version.

  This package is under GPL Copyright, read COPYING file for
  more information about it.


2. INSTALLATION
   ------------

  Unzip the zip file, and copy the content of the directory game to your
  SD card.

  For any comments or questions on this version, please visit 
  http://zx81.zx81.free.fr, http://zx81.dcemu.co.uk or 
  http://www.gp32x.com/


3. CONTROL
   ------------

3.1 - Virtual keyboard

  In the PC-Engine emulator window, there are three different mapping 
  (standard, left trigger, and right Trigger mappings). 
  You can toggle between while playing inside the emulator using 
  the two Dingoo trigger keys.

    -------------------------------------
    Dingoo        PC-Engine          (standard)
  
    Y          Select
    X          Run 
    A          A  
    B          B
    Left       Left
    Down       Down
    Right      Right
    Up         Up

    -------------------------------------
    Dingoo        PC-Engine (left trigger)
  
    Y          FPS  
    X          LOAD Snapshot
    B          Swap digital / Analog
    A          SAVE Snapshot
    Up         Inc delta Y
    Down       Dec delta Y
    Left       Render mode
    Right      Render mode

    -------------------------------------
    Dingoo        PC-Engine (right trigger)
  
    Y          Select
    X          Run
    A          A
    B          Auto-fire
    Up         Up
    Down       Down
    Left       Dec fire
    Right      Inc fire
  
    Press Select to enter in emulator main menu.
    Press Start  open/close the On-Screen keyboard

  In the main menu

    RTrigger   Reset the emulator

    X      Go Up directory
    B      Valid
    A      Valid
    Y      Go Back to the emulator window

  The On-Screen Keyboard of "Danzel" and "Jeff Chen"

  Use digital pad to choose one of the 9 squares, and
  use X, Y, A, B to choose one of the 4 letters of the
  highlighted square.
  
  Use LTrigger and RTrigger to see other 9 squares
  figures.

4. LOADING ROM FILES (.pce)
   -----------------------

  If you want to load rom image in your emulator, you have to put 
  your rom file (with .pce file extension) on your Dingoo
  SD card in the 'roms' directory. 

  Then, while inside emulator, just press SELECT to enter in the emulator 
  main menu, choose "Load Rom", and then using the file selector choose one 
  rom image  file to load in your emulator.

  You can use the virtual keyboard in the file requester menu to choose the
  first letter of the game you search (it might be useful when you have tons of
  games in the same folder). Entering several time the same letter let you
  choose sequentially files beginning with the given letter. You can use the
  Run key of the virtual keyboard to launch the rom.

5. LOADING ISO FILES (.ISO)
   -----------------------

  If you want to load iso image in your emulator, you have to put your file
  (with .iso file extension) on your SD card in the 'cd-roms' directory. 

6. LOADING HCD FILES (.HCD)
   -----------------------

HCD format describes CD tacks for Hu-go. Here is an example of that 
file format:

[main]
first_track=1
last_track=22
minimum_bios=1

[track1]
type=AUDIO
filename=Track01.wav
begin=lsn,0

[track2]
type=CODE
filename=Track02.iso
begin=lsn,3890

....

See Hu-go web site for more details.

If you want to load a hcd file with all track datas in your emulator, you have
to put your file (with .hcd file extension) on your SD card in a
sub-folder of the 'cd-roms' directory. 

For example for Dracula X i have the following folders :

cd-roms
`-- dracx
    |-- Track01.wav
    |-- Track02.iso
    |-- ...
    |-- Track22.iso
    `-- dracx.hcd

An example for dracx.hcd file is present in doc folder.

Audio tracks are not supported in this version so no 
need to put them on your memory card.

7. LOADING TOC FILES (.TOC)
   -----------------------

TOC format describes CD tacks. Here is an example of that 
file format:

Track 01 Audio 00:02:00 LBA=000000
Track 02 Data  00:49:65 LBA=003590
Track 03 Audio 01:27:23 LBA=006398
...

If you want to load a TOC file with all track datas in your emulator, you have
to put your file (with .toc file extension) on your memory card in a
sub-folder of the 'cd-roms' directory. 

For example for R-Type i have the following folders :

cd-roms
`-- r-type
    |-- 01.wav
    |-- 02.iso
    |--...
    |-- 46.iso
    `-- rtype.toc


8. CHEAT CODE (.CHT)
   -----------------

You can use cheat codes with Dingux-Hugo.  You can add your own cheat codes 
in the global cheat.txt file and then import them in the cheat menu.

The cheat.txt file provided with Dingux-Hugo is given as an example, and i
haven't tested all cheat codes. I am pretty sure that most of them won't work.
But see below how to find and create you own cheat code using Dingux-Hugo cheat
engine.

All cheat codes you have specified for a game can be save in a CHT file in
'cht' folder.
Those cheat codes would then be automatically loaded when you start the game.

The CHT file format is the following :
#
# Enable, Address, Value, Comment
#
1,36f,3,Cheat comment

Using the Cheat menu you can search for modified bytes in RAM between current
time and the last time you saved the RAM. It might be very usefull to find
"poke" address by yourself, monitoring for example life numbers.

To find a new "poke address" you can proceed as follow :

Let's say you're playing PC kid and you want to find the memory address where
"number lives" is stored.

. Start a new game in PC kid
. Enter in the cheat menu. 
. Choose Save Ram to save initial state of the memory. 
. Specify the number of lives you want to find in "Scan Old Value" field.
  (for PC kid the initial lives number is 2)
. Go back to the game and loose a life.
. Enter in the cheat menu. 
. Specify the number of lives you want to find in "Scan New Value" field.
  (for PC kid the lives number is now 1)
. In Add Cheat you have now one matching Address
  (for PC kid it's 0DAF)
. Specify the Poke value you want (for example 3) and add a new cheat with
  this address / value.

The cheat is now activated in the cheat list and you can save it using the
"Save cheat" menu.

Let's now enjoy PC kid with infinite life !!


9. LOADING KEY MAPPING FILES
   -------------------------

  For given games, the default keyboard mapping between Dingoo Keys and PC Engine
  keys, is not suitable, and the game can't be played on DingooHUGO.

  To overcome the issue, you can write your own mapping file. Using notepad for
  example you can edit a file with the .kbd extension and put it in the kbd 
  directory.

  For the exact syntax of those mapping files, have a look on sample files already
  presents in the kbd directory (default.kbd etc ...).

  After writting such keyboard mapping file, you can load them using 
  the main menu inside the emulator.

  If the keyboard filename is the same as the rom file then when you load 
  this file, the corresponding keyboard file is automatically loaded !

  You can now use the Keyboard menu and edit, load and save your
  keyboard mapping files inside the emulator. The Save option save the .kbd
  file in the kbd directory using the "Game Name" as filename. The game name
  is displayed on the right corner in the emulator menu.

  If you have saved the state of a game, then a thumbnail image will be
  displayed in the file requester while selecting any file (roms, keyboard,
  settings) with game name, to help you to recognize that game later.

10. COMMENTS
   ------------

  You can write your own comments for games using the "Comment" menu.
  The first line of your comments would then be displayed in the file
  requester menu while selecting the given file name 
  (roms, keyboard, settings)

  
11. SETTINGS
   ------------

  You can modify several settings value in the settings menu of this emulator.
  The following parameters are available :

  Sound enable       : enable or disable the sound
  Sound volume boost : factor to apply to the volume, useful to 
                       increase the sound volume on given game.
  Sound frequency    : sound quality, it could be 22k/44k mono/stereo
  Speed limiter      : limit the speed to a given fps value
  Skip frame         : to skip frame and increase emulator speed
  Overclock          : useful to increase significantly emulator speed
                       but you may encounter graphical glitches
  Display fps        : display real time fps value 
  Render mode        : two render modes are available with different 
                       geometry that should covered all games requirements
  Vsync              : wait for vertical signal between each frame displayed
  Clock frequency    : Dingoo clock frequency

  If you want to modify the default setting, you just need to save the
  settings just after emulator startup. Settings will be saved as default each
  time you restart the emulator. 
   

12. JOYSTICK SETTINGS
   ------------

  You can modify several joystick settings value in the settings menu of this emulator.
  The following parameters are available :

  Active Joystick    : joystick player, it could be 1 or 2
  Swap Analog/Cursor : swap key mapping between Dingux analog pad and Dingux digital pad
  Auto fire period   : auto fire period
  Auto fire mode     : auto fire mode active or not

  See IRDA-Joy section for other parameters description.


13. PERFORMANCES
   ------------

By default the Dingoo clock frequency is set to 400Mhz and it should 
be enough for most of all games. 

Some games such as PC-kid are fullspeed at 300Mhz with overclock parameter
set to 32 and using 22Khz mono sound. If you want to save your battery
and play longuer it might be a good choice.

If you encounter graphical glitches then you may set Overclock value to 0 
and increase the Dingoo clock frequency for a better emulation experience.
  
14. COMPILATION
   ------------

  It has been developped under Linux FC9 using gcc with DINGUX SDK. 
  All tests have been done using a Dingoo with Dingux installed
  To rebuild the homebrew run the Makefile in the src archive.

  Enjoy,

            Zx

