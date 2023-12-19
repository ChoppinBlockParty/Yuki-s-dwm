#!/usr/bin/env bash

SCRIPT_DIR="$(realpath $(dirname $0))"
feh --no-fehbg -Z --recursive --bg-fill --randomize ${SCRIPT_DIR}/wallpapers/*

# In case we  are running vmware
[[ -x /usr/bin/vmware-user ]] && /usr/bin/vmware-user

function run_if_does_not_exist {
  if ! pgrep -f "$1" > /dev/null; then
     eval "nohup $1 2>&1 > /dev/null &"
  fi
}

run_if_does_not_exist "nm-applet"
run_if_does_not_exist "blueman-applet"
if ! pgrep -f "gpaste-daemon" > /dev/null; then
   gpaste-client start
fi

# Not sure I like this anylonger
# Gives some problems in VMWare Player
# run_if_does_not_exist "unclutter"

setxkbmap us,ru
run_if_does_not_exist "kbdd"

# Wait for the keyboard_hook service that sleeps 3 seconds
sleep 3
xset r rate 200 40
xset dpms 600 1200 1800

# if xrandr | grep --color=never "eDP1 connected 1920x1080+0+0" > /dev/null; then
#   if xrandr | grep --color=never "DP2 connected 1920x1080+0+0" > /dev/null; then
#     xrandr --output DP2 --above eDP1 --mode 1920x1080
#   fi
# fi
