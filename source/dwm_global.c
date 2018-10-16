#include "dwm_global.h"

#include "drw.h"

Display* dwm_x_display = {0};
int dwm_x_screen = 0;
int dwm_x_screen_width = 0;
int dwm_x_screen_height = 0;
Window dwm_x_window = {0};

static dwm_drw_t _dwm_drw;
dwm_drw_t* dwm_drw = &_dwm_drw;

dwm_monitor_t* dwm_screens = {0};
dwm_monitor_t* dwm_this_screen = {0};
int dwm_bar_height = 0;
XftColor** dwm_color_schemes = {0};
Atom dwm_x_wm_atoms[_WMLast] = {0};
Atom dwm_x_net_atoms[_NetLast] = {0};
Atom dwm_x_atoms[_XLast] = {0};
