#!/bin/sh
#echo "Install MIDPath..."
#rm -rf /media/data/local/home/midpath/
#mkdir -p /media/data/local/home/midpath/
#cp -ap /mnt/midpath/* /media/data/local/home/midpath/
#cp -ap /media/data/local/home/midpath/30_midpath /media/data/local/home/.gmenu2x/sections/emulators/30_midpath
#rm -rf /usr/local/midpath
#mkdir -p /usr/local/midpath
#cp -ap /media/data/local/home/midpath/phoneme_cdc /usr/local/midpath/
#umount /mnt/midpath/ > /dev/null 2>&1
#rm /media/data/apps/midpath*.opk
#echo "Complete !"

cd /mnt/game/midpath/bin
./midlet-launcher.sh -jar $1
