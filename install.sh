#! /usr/bin/env bash

set -e

# lightdm slick-greeter
sudo apt-get install -y --no-install-recommends xorg libx11-dev libxft-dev fonts-dejavu fonts-freefont-ttf network-manager-gnome gnome-keyring parcellite unclutter kbdd xsel libxinerama-dev

make clean || true
make
sudo make install
mkdir -p ~/.config/dwm
cp -f etc/startup.sh  ~/.config/dwm
