#include "dwm_systray.h"

#include "../config.h"
#include "dwm_enum.h"
#include "dwm_global.h"
#include "util.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <stdlib.h>
#include <stdio.h>

typedef struct {
  Window win;
  /* Client* icons; */
} dwm_systray_t;

static dwm_systray_t* systray;

#define _NET_SYSTEM_TRAY_ORIENTATION_HORZ 0
static unsigned long systrayorientation = _NET_SYSTEM_TRAY_ORIENTATION_HORZ;

dwm_screen_t* dwm_systray_monitor(dwm_screen_t* m) {
  dwm_screen_t* t;
  int i, n;
  if (!systraypinning) {
    if (!m)
      return dwm_this_screen;
    return m == dwm_this_screen ? m : NULL;
  }
  for (n = 1, t = dwm_screens; t && t->next; n++, t = t->next)
    ;
  for (i = 1, t = dwm_screens; t && t->next && i < systraypinning; i++, t = t->next)
    ;
  if (systraypinningfailfirst && n < systraypinning)
    return dwm_screens;
  return t;
}

void dwm_create_systray(int x, int y) {
  if (systray)
    return;

  XSetWindowAttributes wa;
  if (!(systray = (dwm_systray_t*)calloc(1, sizeof(dwm_systray_t))))
    die("fatal: could not calloc() %u bytes\n", sizeof(dwm_systray_t));
  systray->win = XCreateSimpleWindow(dwm_x_display,
                                     dwm_x_window,
                                     x,
                                     y,
                                     1,
                                     dwm_bar_height,
                                     0,
                                     0,
                                     dwm_color_schemes[DwmThisScheme][DwmBgColor].pixel);
  wa.event_mask = ButtonPressMask | ExposureMask;
  wa.override_redirect = True;
  wa.background_pixel = dwm_color_schemes[DwmNormalScheme][DwmBgColor].pixel;
  XSelectInput(dwm_x_display, systray->win, SubstructureNotifyMask);
  XChangeProperty(dwm_x_display,
                  systray->win,
                  dwm_x_net_atoms[NetSystemTrayOrientation],
                  XA_CARDINAL,
                  32,
                  PropModeReplace,
                  (unsigned char*)&systrayorientation,
                  1);
  XChangeWindowAttributes(
    dwm_x_display, systray->win, CWEventMask | CWOverrideRedirect | CWBackPixel, &wa);
  XMapRaised(dwm_x_display, systray->win);
  XSetSelectionOwner(
    dwm_x_display, dwm_x_net_atoms[NetSystemTray], systray->win, CurrentTime);
  if (XGetSelectionOwner(dwm_x_display, dwm_x_net_atoms[NetSystemTray]) == systray->win) {
    sendevent(dwm_x_window,
              dwm_x_atoms[Manager],
              StructureNotifyMask,
              CurrentTime,
              dwm_x_net_atoms[NetSystemTray],
              systray->win,
              0,
              0);
    XSync(dwm_x_display, False);
  } else {
    fprintf(stderr, "dwm: unable to obtain system tray.\n");
    free(systray);
    systray = NULL;
    return;
  }
}
