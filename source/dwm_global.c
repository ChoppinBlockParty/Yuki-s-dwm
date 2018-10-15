#include "dwm_global.h"

Display* dwm_x_display = {0};
Window dwm_x_window = {0};
dwm_screen_t* dwm_screens = {0};
dwm_screen_t* dwm_this_screen = {0};
int dwm_bar_height = 0;
XftColor** dwm_color_schemes = {0};
Atom dwm_x_wm_atoms[_WMLast] = {0};
Atom dwm_x_net_atoms[_NetLast] = {0};
Atom dwm_x_atoms[_XLast] = {0};
