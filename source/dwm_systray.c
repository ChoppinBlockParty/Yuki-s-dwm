#include "dwm_systray.h"

#include "../def_config.h"
#include "drw.h"
#include "dwm_core.h"
#include "dwm_enum.h"
#include "util.h"

#include <X11/Xatom.h>

#include <stdio.h>
#include <stdlib.h>

/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY 0
#define XEMBED_WINDOW_ACTIVATE 1
#define XEMBED_WINDOW_DEACTIVATE 2
#define XEMBED_FOCUS_IN 4
#define XEMBED_MODALITY_ON 10
#define XEMBED_MAPPED (1 << 0)

#define VERSION_MAJOR 0
#define VERSION_MINOR 0
#define XEMBED_EMBEDDED_VERSION (VERSION_MAJOR << 16) | VERSION_MINOR

static Window _systray_window;
dwm_window_t* _systray_icons;

#define _NET_SYSTEM_TRAY_ORIENTATION_HORZ 0
static unsigned long systrayorientation = _NET_SYSTEM_TRAY_ORIENTATION_HORZ;

dwm_window_t* dwm_find_systray_icon_window(Window w) {
  if (!DWM_HAS_SYSTRAY || !w)
    return NULL;
  dwm_window_t* i = _systray_icons;
  for (; i && i->win != w; i = i->next)
    ;
  return i;
}

dwm_screen_t* dwm_find_systray_screen(dwm_screen_t* m) {
  dwm_screen_t* t;
  int i, n;
  if (!DWM_SYSTRAY_PINNING) {
    if (!m)
      return dwm_this_screen;
    return m == dwm_this_screen ? m : NULL;
  }
  for (n = 1, t = dwm_screens; t && t->next; n++, t = t->next)
    ;
  for (i = 1, t = dwm_screens; t && t->next && i < DWM_SYSTRAY_PINNING; i++, t = t->next)
    ;
  if (DWM_SYSTRAY_PINNING_FAIL_FIRST && n < DWM_SYSTRAY_PINNING)
    return dwm_screens;
  return t;
}

unsigned int dwm_calculate_systray_width() {
  unsigned int w = 0;
  dwm_window_t* i;
  if (DWM_HAS_SYSTRAY)
    for (i = _systray_icons; i; w += i->w + DWM_SYSTRAY_SPACING, i = i->next)
      ;
  return w ? w + DWM_SYSTRAY_SPACING : 1;
}

void dwm_create_systray() {
  if (!DWM_HAS_SYSTRAY)
    return;

  dwm_screen_t* m = dwm_find_systray_screen(NULL);
  XSetWindowAttributes wa;

  _systray_window
    = XCreateSimpleWindow(dwm_x_display,
                          dwm_x_window,
                          m->mx + m->mw,
                          m->by,
                          1,
                          dwm_bar_height,
                          0,
                          0,
                          dwm_color_schemes[DwmThisScheme][DwmBgColor].pixel);
  wa.event_mask = ButtonPressMask | ExposureMask;
  wa.override_redirect = True;
  wa.background_pixel = dwm_color_schemes[DwmNormalScheme][DwmBgColor].pixel;
  XSelectInput(dwm_x_display, _systray_window, SubstructureNotifyMask);
  XChangeProperty(dwm_x_display,
                  _systray_window,
                  dwm_x_net_atoms[NetSystemTrayOrientation],
                  XA_CARDINAL,
                  32,
                  PropModeReplace,
                  (unsigned char*)&systrayorientation,
                  1);
  XChangeWindowAttributes(
    dwm_x_display, _systray_window, CWEventMask | CWOverrideRedirect | CWBackPixel, &wa);
  XMapRaised(dwm_x_display, _systray_window);
  XSetSelectionOwner(
    dwm_x_display, dwm_x_net_atoms[NetSystemTray], _systray_window, CurrentTime);
  if (XGetSelectionOwner(dwm_x_display, dwm_x_net_atoms[NetSystemTray])
      == _systray_window) {
    dwm_send_x_event(dwm_x_window,
                     dwm_x_atoms[Manager],
                     StructureNotifyMask,
                     CurrentTime,
                     dwm_x_net_atoms[NetSystemTray],
                     _systray_window,
                     0,
                     0);
    XSync(dwm_x_display, False);
  } else {
    fprintf(stderr, "dwm: unable to obtain system tray.\n");
  }
}

void dwm_update_systray() {
  if (!DWM_HAS_SYSTRAY)
    return;

  XSetWindowAttributes wa;
  XWindowChanges wc;
  dwm_screen_t* m = dwm_find_systray_screen(NULL);
  unsigned int x = m->mx + m->mw;
  unsigned int w = 0;

  for (dwm_window_t* i = _systray_icons; i; i = i->next) {
    /* make sure the background color stays the same */
    wa.background_pixel = dwm_color_schemes[DwmNormalScheme][DwmBgColor].pixel;
    XChangeWindowAttributes(dwm_x_display, i->win, CWBackPixel, &wa);
    XMapRaised(dwm_x_display, i->win);
    w += DWM_SYSTRAY_SPACING;
    i->x = w;
    XMoveResizeWindow(dwm_x_display, i->win, i->x, 0, i->w, i->h);
    w += i->w;
    if (i->mon != m)
      i->mon = m;
  }
  w = w ? w + DWM_SYSTRAY_SPACING : 1;
  x -= w;
  XMoveResizeWindow(dwm_x_display, _systray_window, x, m->by, w, dwm_bar_height);
  wc.x = x;
  wc.y = m->by;
  wc.width = w;
  wc.height = dwm_bar_height;
  wc.stack_mode = Above;
  wc.sibling = m->barwin;
  XConfigureWindow(dwm_x_display,
                   _systray_window,
                   CWX | CWY | CWWidth | CWHeight | CWSibling | CWStackMode,
                   &wc);
  XMapWindow(dwm_x_display, _systray_window);
  XMapSubwindows(dwm_x_display, _systray_window);
  /* redraw background */
  XSetForeground(
    dwm_x_display, drw->gc, dwm_color_schemes[DwmNormalScheme][DwmBgColor].pixel);
  XFillRectangle(dwm_x_display, _systray_window, drw->gc, 0, 0, w, dwm_bar_height);
  XSync(dwm_x_display, False);
}

void dwm_release_systray() {
  if (DWM_HAS_SYSTRAY)
    return;
  XUnmapWindow(dwm_x_display, _systray_window);
  XDestroyWindow(dwm_x_display, _systray_window);
}

int dwm_is_systray_window(Window win) {
  return DWM_HAS_SYSTRAY && win == _systray_window;
}

void dwm_raise_systray(dwm_screen_t* m) {
  if (DWM_HAS_SYSTRAY)
    return;
  if (m == dwm_find_systray_screen(m))
    XMapRaised(dwm_x_display, _systray_window);
}

void dwm_toggle_systray() {
  if (DWM_HAS_SYSTRAY)
    return;
  XWindowChanges wc;
  if (dwm_this_screen->showbar) {
    wc.y = 0;
    if (!dwm_this_screen->topbar)
      wc.y = dwm_this_screen->mh - dwm_bar_height;
  } else {
    wc.y = -dwm_bar_height;
  }
  XConfigureWindow(dwm_x_display, _systray_window, CWY, &wc);
}

dwm_window_t* dwm_add_systray_icon(Window win) {
  dwm_window_t* c = (dwm_window_t*)calloc(1, sizeof(dwm_window_t));
  if (!c)
    die("fatal: could not malloc() %u bytes\n", sizeof(dwm_window_t));
  c->win = win;
  c->mon = dwm_this_screen;
  c->next = _systray_icons;
  _systray_icons = c;
  XWindowAttributes wa;
  XGetWindowAttributes(dwm_x_display, c->win, &wa);
  c->x = c->oldx = c->y = c->oldy = 0;
  c->w = c->oldw = wa.width;
  c->h = c->oldh = wa.height;
  c->oldbw = wa.border_width;
  c->bw = 0;
  c->isfloating = True;
  /* reuse tags field as mapped status */
  c->tags = 1;
  dwm_update_size_hints(c);
  dwm_update_systray_icon_geom(c, wa.width, wa.height);
  XAddToSaveSet(dwm_x_display, c->win);
  XSelectInput(
    dwm_x_display, c->win, StructureNotifyMask | PropertyChangeMask | ResizeRedirectMask);
  XReparentWindow(dwm_x_display, c->win, _systray_window, 0, 0);
  /* use parents background color */
  XSetWindowAttributes swa;
  swa.background_pixel = dwm_color_schemes[DwmNormalScheme][DwmBgColor].pixel;
  XChangeWindowAttributes(dwm_x_display, c->win, CWBackPixel, &swa);
  dwm_send_x_event(c->win,
                   dwm_x_net_atoms[Xembed],
                   StructureNotifyMask,
                   CurrentTime,
                   XEMBED_EMBEDDED_NOTIFY,
                   0,
                   _systray_window,
                   XEMBED_EMBEDDED_VERSION);
  /* FIXME not sure if I have to send these events, too */
  dwm_send_x_event(c->win,
                   dwm_x_net_atoms[Xembed],
                   StructureNotifyMask,
                   CurrentTime,
                   XEMBED_FOCUS_IN,
                   0,
                   _systray_window,
                   XEMBED_EMBEDDED_VERSION);
  dwm_send_systray_icon_window_active(c->win);
  dwm_send_x_event(c->win,
                   dwm_x_net_atoms[Xembed],
                   StructureNotifyMask,
                   CurrentTime,
                   XEMBED_MODALITY_ON,
                   0,
                   _systray_window,
                   XEMBED_EMBEDDED_VERSION);
  XSync(dwm_x_display, False);
  return c;
}

void dwm_update_systray_icon_geom(dwm_window_t* i, int w, int h) {
  if (i) {
    i->h = dwm_bar_height;
    if (w == h)
      i->w = dwm_bar_height;
    else if (h == dwm_bar_height)
      i->w = w;
    else
      i->w = (int)((float)dwm_bar_height * ((float)w / (float)h));
    dwm_apply_size_hints(i, &(i->x), &(i->y), &(i->w), &(i->h), False);
    /* force icons into the systray dimenons if they don't want to */
    if (i->h > dwm_bar_height) {
      if (i->w == i->h)
        i->w = dwm_bar_height;
      else
        i->w = (int)((float)dwm_bar_height * ((float)i->w / (float)i->h));
      i->h = dwm_bar_height;
    }
  }
}

void dwm_update_systray_icon_state(dwm_window_t* i, XPropertyEvent* ev) {
  Atom atom;
  int code = 0;

  if (!DWM_HAS_SYSTRAY || !i || ev->atom != dwm_x_atoms[XembedInfo]
      || ((atom = dwm_get_x_atom_property(i, dwm_x_atoms[XembedInfo])) == None))
    return;

  if (atom == None && XEMBED_MAPPED && !i->tags) {
    i->tags = 1;
    code = XEMBED_WINDOW_ACTIVATE;
    XMapRaised(dwm_x_display, i->win);
    dwm_set_x_window_state(i, NormalState);
  } else if (!(atom == None && XEMBED_MAPPED) && i->tags) {
    i->tags = 0;
    code = XEMBED_WINDOW_DEACTIVATE;
    XUnmapWindow(dwm_x_display, i->win);
    dwm_set_x_window_state(i, WithdrawnState);
  } else
    return;
  dwm_send_x_event(i->win,
                   dwm_x_atoms[Xembed],
                   StructureNotifyMask,
                   CurrentTime,
                   code,
                   0,
                   _systray_window,
                   XEMBED_EMBEDDED_VERSION);
}

void dwm_remove_systray_icon(dwm_window_t* i) {
  if (!DWM_HAS_SYSTRAY || !i)
    return;

  dwm_window_t** ii;
  for (ii = &_systray_icons; *ii && *ii != i; ii = &(*ii)->next)
    ;
  if (ii)
    *ii = i->next;
  free(i);
}

void dwm_send_systray_icon_window_active(Window win) {
  dwm_send_x_event(win,
                   dwm_x_net_atoms[Xembed],
                   StructureNotifyMask,
                   CurrentTime,
                   XEMBED_WINDOW_ACTIVATE,
                   0,
                   _systray_window,
                   XEMBED_EMBEDDED_VERSION);
}
