#pragma once

#include "dwm_global.h"

#include <X11/Xlib.h>

dwm_window_t* dwm_find_systray_icon_window(Window w);

dwm_screen_t* dwm_find_systray_screen(dwm_screen_t* m);

unsigned int dwm_calculate_systray_width();

void dwm_create_systray();

void dwm_update_systray();

void dwm_release_systray();

int dwm_is_systray_window(Window win);

void dwm_raise_systray(dwm_screen_t* m);

void dwm_toggle_systray();

dwm_window_t* dwm_add_systray_icon(Window win);

void dwm_update_systray_icon_geom(dwm_window_t* i, int w, int h);

void dwm_update_systray_icon_state(dwm_window_t* i, XPropertyEvent* ev);

void dwm_remove_systray_icon(dwm_window_t* i);

void dwm_send_systray_icon_window_active(Window win);
