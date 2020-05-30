#!/usr/bin/env bash

# for now disable, have troubles finding why the system startup fails
exit 0

function run_if_does_not_exist {
  if ! pgrep -f "$1" > /dev/null; then
     eval "nohup $1 2>&1 > /dev/null &"
  fi
}

run_if_does_not_exist "nm-applet"
run_if_does_not_exist "xscreensaver -no-splash"
run_if_does_not_exist "parcellite -d"
run_if_does_not_exist "unclutter"
setxkbmap us,ru
run_if_does_not_exist "kbdd"
xset r rate 200 40

# if xrandr | grep --color=never "eDP1 connected 1920x1080+0+0" > /dev/null; then
#   if xrandr | grep --color=never "DP2 connected 1920x1080+0+0" > /dev/null; then
#     xrandr --output DP2 --above eDP1 --mode 1920x1080
#   fi
# fi
