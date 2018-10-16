#include "dwm_core.h"

#include "../def_config.h"

#include "util.h"

#include <X11/Xatom.h>

int dwm_apply_size_hints(dwm_window_t* c, int* x, int* y, int* w, int* h, int interact) {
  int baseismin;
  dwm_screen_t* m = c->mon;

  /* set minimum possible */
  *w = MAX(1, *w);
  *h = MAX(1, *h);
  if (interact) {
    if (*x > dwm_x_screen_width)
      *x = dwm_x_screen_width - WIDTH(c);
    if (*y > dwm_x_screen_height)
      *y = dwm_x_screen_height - HEIGHT(c);
    if (*x + *w + 2 * c->bw < 0)
      *x = 0;
    if (*y + *h + 2 * c->bw < 0)
      *y = 0;
  } else {
    if (*x >= m->wx + m->ww)
      *x = m->wx + m->ww - WIDTH(c);
    if (*y >= m->wy + m->wh)
      *y = m->wy + m->wh - HEIGHT(c);
    if (*x + *w + 2 * c->bw <= m->wx)
      *x = m->wx;
    if (*y + *h + 2 * c->bw <= m->wy)
      *y = m->wy;
  }
  if (*h < dwm_bar_height)
    *h = dwm_bar_height;
  if (*w < dwm_bar_height)
    *w = dwm_bar_height;
  if (DWM_RESIZE_HINTS || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
    /* see last two sentences in ICCCM 4.1.2.3 */
    baseismin = c->basew == c->minw && c->baseh == c->minh;
    if (!baseismin) { /* temporarily remove base dimensions */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for aspect limits */
    if (c->mina > 0 && c->maxa > 0) {
      if (c->maxa < (float)*w / *h)
        *w = *h * c->maxa + 0.5;
      else if (c->mina < (float)*h / *w)
        *h = *w * c->mina + 0.5;
    }
    if (baseismin) { /* increment calculation requires this */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for increment value */
    if (c->incw)
      *w -= *w % c->incw;
    if (c->inch)
      *h -= *h % c->inch;
    /* restore base dimensions */
    *w = MAX(*w + c->basew, c->minw);
    *h = MAX(*h + c->baseh, c->minh);
    if (c->maxw)
      *w = MIN(*w, c->maxw);
    if (c->maxh)
      *h = MIN(*h, c->maxh);
  }
  return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void dwm_update_size_hints(dwm_window_t* c) {
  long msize;
  XSizeHints size;

  if (!XGetWMNormalHints(dwm_x_display, c->win, &size, &msize))
    /* size is uninitialized, ensure that size.flags aren't used */
    size.flags = PSize;
  if (size.flags & PBaseSize) {
    c->basew = size.base_width;
    c->baseh = size.base_height;
  } else if (size.flags & PMinSize) {
    c->basew = size.min_width;
    c->baseh = size.min_height;
  } else
    c->basew = c->baseh = 0;
  if (size.flags & PResizeInc) {
    c->incw = size.width_inc;
    c->inch = size.height_inc;
  } else
    c->incw = c->inch = 0;
  if (size.flags & PMaxSize) {
    c->maxw = size.max_width;
    c->maxh = size.max_height;
  } else
    c->maxw = c->maxh = 0;
  if (size.flags & PMinSize) {
    c->minw = size.min_width;
    c->minh = size.min_height;
  } else if (size.flags & PBaseSize) {
    c->minw = size.base_width;
    c->minh = size.base_height;
  } else
    c->minw = c->minh = 0;
  if (size.flags & PAspect) {
    c->mina = (float)size.min_aspect.y / size.min_aspect.x;
    c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
  } else
    c->maxa = c->mina = 0.0;
  c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
}


Atom dwm_get_x_atom_property(dwm_window_t* c, Atom prop) {
  int di;
  unsigned long dl;
  unsigned char* p = NULL;
  Atom da, atom = None;
  /* FIXME dwm_get_x_atom_property should return the number of items and a pointer to
	 * the stored data instead of this workaround */
  Atom req = XA_ATOM;
  if (prop == dwm_x_atoms[XembedInfo])
    req = dwm_x_atoms[XembedInfo];

  if (XGetWindowProperty(
        dwm_x_display, c->win, prop, 0L, sizeof atom, False, req, &da, &di, &dl, &dl, &p)
        == Success
      && p) {
    atom = *(Atom*)p;
    if (da == dwm_x_atoms[XembedInfo] && dl == 2)
      atom = ((Atom*)p)[1];
    XFree(p);
  }
  return atom;
}

int dwm_send_x_event(
  Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4) {
  int n;
  Atom *protocols, mt;
  int exists = 0;
  XEvent ev;

  if (proto == dwm_x_wm_atoms[WMTakeFocus] || proto == dwm_x_wm_atoms[WMDelete]) {
    mt = dwm_x_wm_atoms[WMProtocols];
    if (XGetWMProtocols(dwm_x_display, w, &protocols, &n)) {
      while (!exists && n--)
        exists = protocols[n] == proto;
      XFree(protocols);
    }
  } else {
    exists = True;
    mt = proto;
  }
  if (exists) {
    ev.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = mt;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = d0;
    ev.xclient.data.l[1] = d1;
    ev.xclient.data.l[2] = d2;
    ev.xclient.data.l[3] = d3;
    ev.xclient.data.l[4] = d4;
    XSendEvent(dwm_x_display, w, False, mask, &ev);
  }
  return exists;
}

void dwm_set_x_window_state(dwm_window_t* c, long state) {
  long data[] = {state, None};

  XChangeProperty(dwm_x_display,
                  c->win,
                  dwm_x_wm_atoms[WMState],
                  dwm_x_wm_atoms[WMState],
                  32,
                  PropModeReplace,
                  (unsigned char*)data,
                  2);
}
