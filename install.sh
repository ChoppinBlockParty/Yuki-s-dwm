#! /usr/bin/env bash

set -e

sudo apt-get install -y --no-install-recommends libx11-dev libxft-dev fonts-dejavu fonts-freefont-ttf lightdm slick-greeter network-manager-gnome gnome-keyring parcellite unclutter kbdd xsel libxinerama-dev

if [ -x "$(command -v clang 2>/dev/null)" ]; then
  export CC=clang
  export CXX=clang++
  export AR=llvm-ar
  export RANLIB=llvm-ranlib
fi

make clean || true
make
sudo make install
mkdir -p ~/.config/dwm
cp -f etc/startup.sh  ~/.config/dwm
sudo cp -f etc/startup-service.sh  /etc
sudo cp -f etc/startup.service /etc/systemd/system
sudo systemctl enable startup
sudo systemctl start startup
