#! /usr/bin/env bash

set -e

# lightdm slick-greeter
sudo apt-get install -y --no-install-recommends \
     xorg \
     libx11-dev \
     libxft-dev \
     fonts-dejavu \
     fonts-freefont-ttf \
     network-manager-gnome \
     gnome-keyring \
     gpaste \
     unclutter \
     kbdd \
     xsel \
     feh \
     libxinerama-dev

make clean || true
make
sudo make install
mkdir -p ~/.config/dwm
cp -f etc/startup.sh  ~/.config/dwm

cp wallpapers/artwork-fantasy-art-digital-art-forest.jpg /home/yuki/.config/dwm/wallpapers
cp wallpapers/psychedelic-abstract-creature-trippy.jpg /home/yuki/.config/dwm/wallpapers
cp wallpapers/psychedelic-trippy-colorful-creature.jpg /home/yuki/.config/dwm/wallpapers
cp wallpapers/tree.jpg /home/yuki/.config/dwm/wallpapers
