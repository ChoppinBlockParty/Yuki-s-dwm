#include "dwm_global.h"

#include "drw.h"

Display* dwm_x_display = {0};
int dwm_x_screen = 0;
int dwm_x_screen_width = 0;
int dwm_x_screen_height = 0;
Window dwm_x_window = {0};

static Drw _dwm_drw;
Drw* drw = &_dwm_drw;

dwm_screen_t* dwm_screens = {0};
dwm_screen_t* dwm_this_screen = {0};
int dwm_bar_height = 0;
XftColor** dwm_color_schemes = {0};
Atom dwm_x_wm_atoms[_WMLast] = {0};
Atom dwm_x_net_atoms[_NetLast] = {0};
Atom dwm_x_atoms[_XLast] = {0};
