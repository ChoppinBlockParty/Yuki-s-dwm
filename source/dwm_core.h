#pragma once

#include "dwm_global.h"

#include <X11/Xlib.h>

int dwm_apply_size_hints(dwm_window_t* c, int* x, int* y, int* w, int* h, int interact);

void dwm_update_size_hints(dwm_window_t* c);

Atom dwm_get_x_atom_property(dwm_window_t* c, Atom prop);

int dwm_send_x_event(
  Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4);

void dwm_set_x_window_state(dwm_window_t* c, long state);
