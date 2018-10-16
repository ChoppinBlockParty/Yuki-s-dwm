/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the dwm_x_window window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>

#include "drw.h"
#include "dwm.h"
#include "dwm_core.h"
#include "dwm_enum.h"
#include "dwm_global.h"
#include "dwm_systray.h"
#include "util.h"

/* macros */
#define BUTTONMASK (ButtonPressMask | ButtonReleaseMask)
#define CLEANMASK(mask)                                                                  \
  (mask & ~(numlockmask | LockMask)                                                      \
   & (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask))
#define INTERSECT(x, y, w, h, m)                                                         \
  (MAX(0, MIN((x) + (w), (m)->wx + (m)->ww) - MAX((x), (m)->wx))                         \
   * MAX(0, MIN((y) + (h), (m)->wy + (m)->wh) - MAX((y), (m)->wy)))
#define ISVISIBLE(C) ((C->tags & C->mon->tagset[C->mon->seltags]))
#define MOUSEMASK (BUTTONMASK | PointerMotionMask)
#define TAGMASK ((1 << LENGTH(tags)) - 1)
#define TEXTW(X) (dwm_drw_fontset_getwidth(dwm_drw, (X)) + lrpad)

#define SYSTEM_TRAY_REQUEST_DOCK 0

/* variables */
static const char broken[] = "broken";
static char stext[256];
static int blw = 0; /* bar geometry */
static int lrpad; /* sum of left and right padding for text */
static int (*xerrorxlib)(Display*, XErrorEvent*);
static unsigned int numlockmask = 0;
static void (*drw_x_event_handlers[LASTEvent])(XEvent*)
  = {[ButtonPress] = buttonpress,
     [ClientMessage] = clientmessage,
     [ConfigureRequest] = configurerequest,
     [ConfigureNotify] = configurenotify,
     [DestroyNotify] = destroynotify,
     [EnterNotify] = enternotify,
     [Expose] = expose,
     [FocusIn] = focusin,
     [KeyPress] = keypress,
     [MappingNotify] = mappingnotify,
     [MapRequest] = maprequest,
     [MotionNotify] = motionnotify,
     [PropertyNotify] = propertynotify,
     [ResizeRequest] = resizerequest,
     [UnmapNotify] = unmapnotify};
static int running = 1;
static Cur* cursor[CurLast];
static Window wmcheckwin;

/* configuration, allows nested code to access above variables */
#include "../config.h"
#include "../def_config.h"

static unsigned int scratchtag = 1 << LENGTH(tags);

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags {
  // +1 is for scratchtag
  char limitexceeded[(LENGTH(tags) + 1) > 31 ? -1 : 1];
};

/* function implementations */
void dwm_log(char const* str) {
  FILE* f = fopen("/tmp/dwm.log", "a+");
  if (!f)
    return;
  fprintf(f, "%s\n", str);
  fclose(f);
}

static unsigned char const* get_window_property_data_and_type(
  Atom atom, Window target_window, long* length, Atom* type, int* size) {
  Atom actual_type;
  int actual_format;
  unsigned long nitems;
  unsigned long nbytes;
  unsigned long bytes_after;
  unsigned char* prop;
  int status;

  unsigned long max_len = 1024 - 1;
  status = XGetWindowProperty(dwm_x_display,
                              target_window,
                              atom,
                              0,
                              max_len,
                              False,
                              AnyPropertyType,
                              &actual_type,
                              &actual_format,
                              &nitems,
                              &bytes_after,
                              &prop);
  if (status == BadWindow)
    return NULL;
  if (status != Success)
    return NULL;

  if (actual_format == 32)
    nbytes = sizeof(long);
  else if (actual_format == 16)
    nbytes = sizeof(short);
  else if (actual_format == 8)
    nbytes = 1;
  else if (actual_format == 0)
    nbytes = 0;
  else
    return NULL;
  *length = nitems * nbytes;
  if (*length > max_len)
    *length = max_len;
  *type = actual_type;
  *size = actual_format;
  return prop;
}

unsigned long get_pid(Window target_window) {
  // https://specifications.freedesktop.org/wm-spec/wm-spec-latest.html#idm140200472565744
  const char* name = "_NET_WM_PID";

  long length;
  int size;
  Atom type;

  Atom atom = XInternAtom(dwm_x_display, name, True);
  if (atom == None) {
    return 0;
  }

  unsigned char const* prop
    = get_window_property_data_and_type(atom, target_window, &length, &type, &size);
  if (!prop || length != 8)
    return 0;
  unsigned long pid = *(unsigned long*)prop;
  return pid;
}

void applyrules(dwm_client_t* c) {
  const char* class, *instance;
  unsigned int i;
  const Rule* r;
  dwm_monitor_t* m;
  XClassHint ch = {NULL, NULL};

  /* rule matching */
  c->isfloating = 0;
  c->tags = 0;
  XGetClassHint(dwm_x_display, c->win, &ch);
  class = ch.res_class ? ch.res_class : broken;
  instance = ch.res_name ? ch.res_name : broken;

  for (i = 0; i < LENGTH(rules); i++) {
    r = &rules[i];
    if ((!r->title || strstr(c->name, r->title)) && (!r->class || strstr(class, r->class))
        && (!r->instance || strstr(instance, r->instance))) {
      c->isfloating = r->isfloating;
      c->tags |= r->tags;
      for (m = dwm_screens; m && m->num != r->monitor; m = m->next)
        ;
      if (m)
        c->mon = m;
    }
  }
  if (ch.res_class)
    XFree(ch.res_class);
  if (ch.res_name)
    XFree(ch.res_name);
  c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

void arrange(dwm_monitor_t* m) {
  if (m)
    showhide(m->stack);
  else
    for (m = dwm_screens; m; m = m->next)
      showhide(m->stack);
  if (m) {
    arrangemon(m);
    restack(m);
  } else
    for (m = dwm_screens; m; m = m->next)
      arrangemon(m);
}

void arrangemon(dwm_monitor_t* m) {
  strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
  if (m->lt[m->sellt]->arrange)
    m->lt[m->sellt]->arrange(m);
}

void attach(dwm_client_t* c) {
  c->next = c->mon->clients;
  c->mon->clients = c;
}

void attachstack(dwm_client_t* c) {
  c->snext = c->mon->stack;
  c->mon->stack = c;
}

void buttonpress(XEvent* e) {
  unsigned int i, x, click;
  Arg arg = {0};
  dwm_client_t* c;
  dwm_monitor_t* m;
  XButtonPressedEvent* ev = &e->xbutton;

  click = ClkRootWin;
  /* focus monitor if necessary */
  if ((m = wintomon(ev->window)) && m != dwm_this_screen) {
    unfocus(dwm_this_screen->sel, 1);
    dwm_this_screen = m;
    focus(NULL);
  }
  if (ev->window == dwm_this_screen->barwin) {
    i = x = 0;
    do
      x += TEXTW(tags[i]);
    while (ev->x >= x && ++i < LENGTH(tags));
    if (i < LENGTH(tags)) {
      click = ClkTagBar;
      arg.ui = 1 << i;
    } else if (ev->x < x + blw)
      click = ClkLtSymbol;
    else if (ev->x > dwm_this_screen->ww - TEXTW(stext))
      click = ClkStatusText;
    else
      click = ClkWinTitle;
  } else if ((c = wintoclient(ev->window))) {
    focus(c);
    restack(dwm_this_screen);
    XAllowEvents(dwm_x_display, ReplayPointer, CurrentTime);
    click = ClkClientWin;
  }
  for (i = 0; i < LENGTH(buttons); i++)
    if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
        && CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
      buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg
                                                                  : &buttons[i].arg);
}

void checkotherwm(void) {
  xerrorxlib = XSetErrorHandler(xerrorstart);
  /* this causes an error if some other window manager is running */
  XSelectInput(dwm_x_display, DefaultRootWindow(dwm_x_display), SubstructureRedirectMask);
  XSync(dwm_x_display, False);
  XSetErrorHandler(xerror);
  XSync(dwm_x_display, False);
}

void cleanup(void) {
  Arg a = {.ui = ~0};
  dwm_layout_t foo = {"", NULL};
  dwm_monitor_t* m;
  size_t i;

  view(&a);
  dwm_this_screen->lt[dwm_this_screen->sellt] = &foo;
  for (m = dwm_screens; m; m = m->next)
    while (m->stack)
      unmanage(m->stack, 0);
  XUngrabKey(dwm_x_display, AnyKey, AnyModifier, dwm_x_window);
  while (dwm_screens)
    cleanupmon(dwm_screens);

  dwm_release_systray();
  for (i = 0; i < CurLast; i++)
    dwm_drw_cur_free(dwm_drw, cursor[i]);
  for (i = 0; i < LENGTH(colors); i++)
    free(dwm_color_schemes[i]);
  XDestroyWindow(dwm_x_display, wmcheckwin);
  dwm_release_dwm_drw(dwm_drw);
  XSync(dwm_x_display, False);
  XSetInputFocus(dwm_x_display, PointerRoot, RevertToPointerRoot, CurrentTime);
  XDeleteProperty(dwm_x_display, dwm_x_window, dwm_x_net_atoms[NetActiveWindow]);
}

void cleanupmon(dwm_monitor_t* mon) {
  dwm_monitor_t* m;

  if (mon == dwm_screens)
    dwm_screens = dwm_screens->next;
  else {
    for (m = dwm_screens; m && m->next != mon; m = m->next)
      ;
    m->next = mon->next;
  }
  XUnmapWindow(dwm_x_display, mon->barwin);
  XDestroyWindow(dwm_x_display, mon->barwin);
  free(mon);
}

void clientmessage(XEvent* e) {
  XClientMessageEvent* cme = &e->xclient;

  if (dwm_is_systray_window(cme->window)
      && cme->message_type == dwm_x_net_atoms[NetSystemTrayOP]) {
    if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
      dwm_client_t* c = dwm_add_systray_icon(cme->data.l[2]);
      move_resize_bar(dwm_this_screen);
      dwm_update_systray();
      dwm_set_x_window_state(c, NormalState);
    }
    return;
  }

  dwm_client_t* c = wintoclient(cme->window);

  if (!c)
    return;
  if (cme->message_type == dwm_x_net_atoms[NetWMState]) {
    if (cme->data.l[1] == dwm_x_net_atoms[NetWMFullscreen]
        || cme->data.l[2] == dwm_x_net_atoms[NetWMFullscreen])
      setfullscreen(
        c,
        (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
         || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
  } else if (cme->message_type == dwm_x_net_atoms[NetActiveWindow]) {
    if (c != dwm_this_screen->sel && !c->isurgent)
      seturgent(c, 1);
  }
}

void configure(dwm_client_t* c) {
  XConfigureEvent ce;

  ce.type = ConfigureNotify;
  ce.display = dwm_x_display;
  ce.event = c->win;
  ce.window = c->win;
  ce.x = c->x;
  ce.y = c->y;
  ce.width = c->w;
  ce.height = c->h;
  ce.border_width = c->bw;
  ce.above = None;
  ce.override_redirect = False;
  XSendEvent(dwm_x_display, c->win, False, StructureNotifyMask, (XEvent*)&ce);
}

void configurenotify(XEvent* e) {
  dwm_monitor_t* m;
  dwm_client_t* c;
  XConfigureEvent* ev = &e->xconfigure;
  int dirty;

  /* TODO: updategeom handling sucks, needs to be simplified */
  if (ev->window == dwm_x_window) {
    dirty = (dwm_x_screen_width != ev->width || dwm_x_screen_height != ev->height);
    dwm_x_screen_width = ev->width;
    dwm_x_screen_height = ev->height;
    if (updategeom() || dirty) {
      dwm_drw_resize(dwm_drw, dwm_x_screen_width, dwm_bar_height);
      updatebars();
      for (m = dwm_screens; m; m = m->next) {
        for (c = m->clients; c; c = c->next)
          if (c->isfullscreen)
            resizeclient(c, m->mx, m->my, m->mw, m->mh);
        move_resize_bar(m);
      }
      focus(NULL);
      arrange(NULL);
    }
  }
}

void configurerequest(XEvent* e) {
  dwm_client_t* c;
  dwm_monitor_t* m;
  XConfigureRequestEvent* ev = &e->xconfigurerequest;
  XWindowChanges wc;

  if ((c = wintoclient(ev->window))) {
    if (ev->value_mask & CWBorderWidth)
      c->bw = ev->border_width;
    else if (c->isfloating || !dwm_this_screen->lt[dwm_this_screen->sellt]->arrange) {
      m = c->mon;
      if (ev->value_mask & CWX) {
        c->oldx = c->x;
        c->x = m->mx + ev->x;
      }
      if (ev->value_mask & CWY) {
        c->oldy = c->y;
        c->y = m->my + ev->y;
      }
      if (ev->value_mask & CWWidth) {
        c->oldw = c->w;
        c->w = ev->width;
      }
      if (ev->value_mask & CWHeight) {
        c->oldh = c->h;
        c->h = ev->height;
      }
      if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
        c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
      if ((c->y + c->h) > m->my + m->mh && c->isfloating)
        c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
      if ((ev->value_mask & (CWX | CWY)) && !(ev->value_mask & (CWWidth | CWHeight)))
        configure(c);
      if (ISVISIBLE(c))
        XMoveResizeWindow(dwm_x_display, c->win, c->x, c->y, c->w, c->h);
    } else
      configure(c);
  } else {
    wc.x = ev->x;
    wc.y = ev->y;
    wc.width = ev->width;
    wc.height = ev->height;
    wc.border_width = ev->border_width;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(dwm_x_display, ev->window, ev->value_mask, &wc);
  }
  XSync(dwm_x_display, False);
}

dwm_monitor_t* createmon(void) {
  dwm_monitor_t* m;

  m = ecalloc(1, sizeof(dwm_monitor_t));
  m->tagset[0] = m->tagset[1] = 1;
  m->mfact = mfact;
  m->nmaster = nmaster;
  m->showbar = showbar;
  m->topbar = topbar;
  m->lt[0] = &layouts[0];
  m->lt[1] = &layouts[1 % LENGTH(layouts)];
  strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
  return m;
}

void destroynotify(XEvent* e) {
  dwm_client_t* c;
  XDestroyWindowEvent* ev = &e->xdestroywindow;

  if ((c = wintoclient(ev->window))) {
    unmanage(c, 1);
  } else if ((c = dwm_find_systray_icon_window(ev->window))) {
    dwm_remove_systray_icon(c);
    move_resize_bar(dwm_this_screen);
    dwm_update_systray();
  }
}

void detach(dwm_client_t* c) {
  dwm_client_t** tc;

  for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next)
    ;
  *tc = c->next;

  if (c == c->mon->scratchpad) {
    c->mon->scratchpad = NULL;
  }
}

void detachstack(dwm_client_t* c) {
  dwm_client_t **tc, *t;

  for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext)
    ;
  *tc = c->snext;

  if (c == c->mon->sel) {
    for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext)
      ;
    c->mon->sel = t;
  }
}

dwm_monitor_t* dirtomon(int dir) {
  dwm_monitor_t* m = NULL;

  if (dir > 0) {
    if (!(m = dwm_this_screen->next))
      m = dwm_screens;
  } else if (dwm_this_screen == dwm_screens)
    for (m = dwm_screens; m->next; m = m->next)
      ;
  else
    for (m = dwm_screens; m->next != dwm_this_screen; m = m->next)
      ;
  return m;
}

void drawbar(dwm_monitor_t* m) {
  int x, w, dwm_x_screen_width = 0;
  int boxs = dwm_drw->fonts->h / 9;
  int boxw = dwm_drw->fonts->h / 6 + 2;
  unsigned int i, occ = 0, urg = 0;
  dwm_client_t* c;

  /* draw status first so it can be overdrawn by tags later */
  if (m == dwm_this_screen) { /* status is only drawn on selected monitor */
    if (DWM_HAS_SYSTRAY && m == dwm_find_systray_screen(m)) {
      dwm_x_screen_width = dwm_calculate_systray_width();
    }
  }

  move_resize_bar(m);
  for (c = m->clients; c; c = c->next) {
    occ |= c->tags;
    if (c->isurgent)
      urg |= c->tags;
  }
  x = 0;
  for (i = 0; i < LENGTH(tags); i++) {
    w = TEXTW(tags[i]);
    dwm_drw_setscheme(
      dwm_drw,
      dwm_color_schemes[m->tagset[m->seltags] & 1 << i ? DwmThisScheme
                                                       : DwmNormalScheme]);
    dwm_drw_text(dwm_drw, x, 0, w, dwm_bar_height, lrpad / 2, tags[i], urg & 1 << i);
    if (occ & 1 << i)
      dwm_drw_rect(dwm_drw,
                   x + boxs,
                   boxs,
                   boxw,
                   boxw,
                   m == dwm_this_screen && dwm_this_screen->sel
                     && dwm_this_screen->sel->tags & 1 << i,
                   urg & 1 << i);
    x += w;
  }
  w = blw = TEXTW(m->ltsymbol);
  dwm_drw_setscheme(dwm_drw, dwm_color_schemes[DwmNormalScheme]);
  x = dwm_drw_text(dwm_drw, x, 0, w, dwm_bar_height, lrpad / 2, m->ltsymbol, 0);

  if ((w = m->ww - dwm_x_screen_width - x) > dwm_bar_height) {
    if (m->sel) {
      dwm_drw_setscheme(
        dwm_drw,
        dwm_color_schemes[m == dwm_this_screen ? DwmThisScheme : DwmNormalScheme]);
      dwm_drw_text(dwm_drw, x, 0, w, dwm_bar_height, lrpad / 2, m->sel->name, 0);
      if (m->sel->isfloating)
        dwm_drw_rect(dwm_drw, x + boxs, boxs, boxw, boxw, m->sel->isfixed, 0);
    } else {
      dwm_drw_setscheme(dwm_drw, dwm_color_schemes[DwmNormalScheme]);
      dwm_drw_rect(dwm_drw, x, 0, w, dwm_bar_height, 1, 1);
    }
  }
  dwm_drw_map(dwm_drw, m->barwin, 0, 0, m->ww, dwm_bar_height);
}

void drawbars(void) {
  dwm_monitor_t* m;

  for (m = dwm_screens; m; m = m->next)
    drawbar(m);
  dwm_update_systray();
}

void enternotify(XEvent* e) {
  dwm_client_t* c;
  dwm_monitor_t* m;
  XCrossingEvent* ev = &e->xcrossing;

  if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior)
      && ev->window != dwm_x_window)
    return;
  c = wintoclient(ev->window);
  m = c ? c->mon : wintomon(ev->window);
  if (m != dwm_this_screen) {
    unfocus(dwm_this_screen->sel, 1);
    dwm_this_screen = m;
  } else if (!c || c == dwm_this_screen->sel)
    return;
  focus(c);
}

void expose(XEvent* e) {
  dwm_monitor_t* m;
  XExposeEvent* ev = &e->xexpose;

  if (ev->count == 0 && (m = wintomon(ev->window))) {
    drawbar(m);
    if (m == dwm_this_screen)
      dwm_update_systray();
  }
}

void focus(dwm_client_t* c) {
  if (!c || !ISVISIBLE(c))
    for (c = dwm_this_screen->stack; c && !ISVISIBLE(c); c = c->snext)
      ;
  if (dwm_this_screen->sel && dwm_this_screen->sel != c)
    unfocus(dwm_this_screen->sel, 0);
  if (c) {
    if (c->mon != dwm_this_screen)
      dwm_this_screen = c->mon;
    if (c->isurgent)
      seturgent(c, 0);
    detachstack(c);
    attachstack(c);
    grabbuttons(c, 1);
    XSetWindowBorder(
      dwm_x_display, c->win, dwm_color_schemes[DwmThisScheme][DwmBorderColor].pixel);
    setfocus(c);
  } else {
    XSetInputFocus(dwm_x_display, dwm_x_window, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dwm_x_display, dwm_x_window, dwm_x_net_atoms[NetActiveWindow]);
  }
  dwm_this_screen->sel = c;
  drawbars();
}

/* there are some broken focus acquiring clients needing extra handling */
void focusin(XEvent* e) {
  XFocusChangeEvent* ev = &e->xfocus;

  if (dwm_this_screen->sel && ev->window != dwm_this_screen->sel->win)
    setfocus(dwm_this_screen->sel);
}

void focusmon(const Arg* arg) {
  dwm_monitor_t* m;

  if (!dwm_screens->next)
    return;
  if ((m = dirtomon(arg->i)) == dwm_this_screen)
    return;
  unfocus(dwm_this_screen->sel, 0);
  dwm_this_screen = m;
  focus(NULL);
}

void focusstack(const Arg* arg) {
  dwm_client_t *c = NULL, *i;

  if (!dwm_this_screen->sel)
    return;
  if (arg->i > 0) {
    for (c = dwm_this_screen->sel->next; c && !ISVISIBLE(c); c = c->next)
      ;
    if (!c)
      for (c = dwm_this_screen->clients; c && !ISVISIBLE(c); c = c->next)
        ;
  } else {
    for (i = dwm_this_screen->clients; i != dwm_this_screen->sel; i = i->next)
      if (ISVISIBLE(i))
        c = i;
    if (!c)
      for (; i; i = i->next)
        if (ISVISIBLE(i))
          c = i;
  }
  if (c) {
    focus(c);
    restack(dwm_this_screen);
  }
}

int getrootptr(int* x, int* y) {
  int di;
  unsigned int dui;
  Window dummy;

  return XQueryPointer(dwm_x_display, dwm_x_window, &dummy, &dummy, x, y, &di, &di, &dui);
}

long getstate(Window w) {
  int format;
  long result = -1;
  unsigned char* p = NULL;
  unsigned long n, extra;
  Atom real;

  if (XGetWindowProperty(dwm_x_display,
                         w,
                         dwm_x_wm_atoms[WMState],
                         0L,
                         2L,
                         False,
                         dwm_x_wm_atoms[WMState],
                         &real,
                         &format,
                         &n,
                         &extra,
                         (unsigned char**)&p)
      != Success)
    return -1;
  if (n != 0)
    result = *p;
  XFree(p);
  return result;
}

int gettextprop(Window w, Atom atom, char* text, unsigned int size) {
  char** list = NULL;
  int n;
  XTextProperty name;

  if (!text || size == 0)
    return 0;
  text[0] = '\0';
  if (!XGetTextProperty(dwm_x_display, w, &name, atom) || !name.nitems)
    return 0;
  if (name.encoding == XA_STRING)
    strncpy(text, (char*)name.value, size - 1);
  else {
    if (XmbTextPropertyToTextList(dwm_x_display, &name, &list, &n) >= Success && n > 0
        && *list) {
      strncpy(text, *list, size - 1);
      XFreeStringList(list);
    }
  }
  text[size - 1] = '\0';
  XFree(name.value);
  return 1;
}

void grabbuttons(dwm_client_t* c, int focused) {
  updatenumlockmask();
  {
    unsigned int i, j;
    unsigned int modifiers[] = {0, LockMask, numlockmask, numlockmask | LockMask};
    XUngrabButton(dwm_x_display, AnyButton, AnyModifier, c->win);
    if (!focused)
      XGrabButton(dwm_x_display,
                  AnyButton,
                  AnyModifier,
                  c->win,
                  False,
                  BUTTONMASK,
                  GrabModeSync,
                  GrabModeSync,
                  None,
                  None);
    for (i = 0; i < LENGTH(buttons); i++)
      if (buttons[i].click == ClkClientWin)
        for (j = 0; j < LENGTH(modifiers); j++)
          XGrabButton(dwm_x_display,
                      buttons[i].button,
                      buttons[i].mask | modifiers[j],
                      c->win,
                      False,
                      BUTTONMASK,
                      GrabModeAsync,
                      GrabModeSync,
                      None,
                      None);
  }
}

void grabkeys(void) {
  updatenumlockmask();
  {
    unsigned int i, j;
    unsigned int modifiers[] = {0, LockMask, numlockmask, numlockmask | LockMask};
    KeyCode code;

    XUngrabKey(dwm_x_display, AnyKey, AnyModifier, dwm_x_window);
    for (i = 0; i < LENGTH(keys); i++)
      if ((code = XKeysymToKeycode(dwm_x_display, keys[i].keysym)))
        for (j = 0; j < LENGTH(modifiers); j++)
          XGrabKey(dwm_x_display,
                   code,
                   keys[i].mod | modifiers[j],
                   dwm_x_window,
                   True,
                   GrabModeAsync,
                   GrabModeAsync);
  }
}

void incnmaster(const Arg* arg) {
  dwm_this_screen->nmaster = MAX(dwm_this_screen->nmaster + arg->i, 0);
  arrange(dwm_this_screen);
}

#ifdef XINERAMA
static int isuniquegeom(XineramaScreenInfo* unique, size_t n, XineramaScreenInfo* info) {
  while (n--)
    if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
        && unique[n].width == info->width
        && unique[n].height == info->height)
      return 0;
  return 1;
}
#endif /* XINERAMA */

void keypress(XEvent* e) {
  unsigned int i;
  KeySym keysym;
  XKeyEvent* ev;

  ev = &e->xkey;
  keysym = XkbKeycodeToKeysym(dwm_x_display, (KeyCode)ev->keycode, 0, 0);
  for (i = 0; i < LENGTH(keys); i++)
    if (keysym == keys[i].keysym && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
        && keys[i].func)
      keys[i].func(&(keys[i].arg));
}

void killclient(const Arg* arg) {
  if (!dwm_this_screen->sel)
    return;
  if (!dwm_send_x_event(dwm_this_screen->sel->win,
                        dwm_x_wm_atoms[WMDelete],
                        NoEventMask,
                        dwm_x_wm_atoms[WMDelete],
                        CurrentTime,
                        0,
                        0,
                        0)) {
    XGrabServer(dwm_x_display);
    XSetErrorHandler(xerrordummy);
    XSetCloseDownMode(dwm_x_display, DestroyAll);
    XKillClient(dwm_x_display, dwm_this_screen->sel->win);
    XSync(dwm_x_display, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dwm_x_display);
  }
}

dwm_monitor_t* get_scratchpad_monitor(Window w) {
  unsigned long pid = get_pid(w);
  if (pid) {
    dwm_monitor_t* m = dwm_screens;
    for (; m && m->scratchpadpid != pid; m = m->next)
      ;
    return m;
  }
  return NULL;
}

void manage(Window w, XWindowAttributes* wa) {
  dwm_client_t *c, *t = NULL;
  Window trans = None;
  XWindowChanges wc;
  dwm_monitor_t* scratchpadmon = get_scratchpad_monitor(w);

  c = ecalloc(1, sizeof(dwm_client_t));
  c->win = w;
  /* geometry */
  c->x = c->oldx = wa->x;
  c->y = c->oldy = wa->y;
  c->w = c->oldw = wa->width;
  c->h = c->oldh = wa->height;
  c->oldbw = wa->border_width;

  updatetitle(c);
  if (XGetTransientForHint(dwm_x_display, w, &trans) && (t = wintoclient(trans))) {
    c->mon = t->mon;
    c->tags = t->tags;
  } else if (scratchpadmon) {
    c->mon = scratchpadmon;
    scratchpadmon->scratchpad = c;
    scratchpadmon->scratchpadpid = 0;
  } else {
    c->mon = dwm_this_screen;
    applyrules(c);
  }

  if (scratchpadmon) {
    c->mon->tagset[c->mon->seltags] |= c->tags = scratchtag;
    c->isfloating = True;
    int width = c->mon->ww / 2 + 1;
    int height = c->mon->wh / 2 + 1;
    c->x = c->mon->wx + (c->mon->ww / 2 - width / 2 - borderpx);
    c->y = c->mon->wy + (c->mon->wh / 2 - height / 2 - borderpx);
    c->w = width;
    c->h = height;
    c->bw = borderpx;
  } else {
    if (c->x + WIDTH(c) > c->mon->mx + c->mon->mw)
      c->x = c->mon->mx + c->mon->mw - WIDTH(c);
    if (c->y + HEIGHT(c) > c->mon->my + c->mon->mh)
      c->y = c->mon->my + c->mon->mh - HEIGHT(c);
    c->x = MAX(c->x, c->mon->mx);
    /* only fix client y-offset, if the client center might cover the bar */
    c->y = MAX(c->y,
               ((c->mon->by == c->mon->my) && (c->x + (c->w / 2) >= c->mon->wx)
                && (c->x + (c->w / 2) < c->mon->wx + c->mon->ww))
                 ? dwm_bar_height
                 : c->mon->my);
    c->bw = borderpx;
  }

  wc.border_width = c->bw;
  XConfigureWindow(dwm_x_display, w, CWBorderWidth, &wc);
  XSetWindowBorder(
    dwm_x_display, w, dwm_color_schemes[DwmNormalScheme][DwmBorderColor].pixel);
  configure(c); /* propagates border_width, if size doesn't change */
  updatewindowtype(c);
  dwm_update_size_hints(c);
  updatewmhints(c);
  XSelectInput(dwm_x_display,
               w,
               EnterWindowMask | FocusChangeMask | PropertyChangeMask
                 | StructureNotifyMask);
  grabbuttons(c, 0);
  if (!c->isfloating)
    c->isfloating = c->oldstate = trans != None || c->isfixed;
  if (c->isfloating)
    XRaiseWindow(dwm_x_display, c->win);
  attach(c);
  attachstack(c);
  XChangeProperty(dwm_x_display,
                  dwm_x_window,
                  dwm_x_net_atoms[NetClientList],
                  XA_WINDOW,
                  32,
                  PropModeAppend,
                  (unsigned char*)&(c->win),
                  1);
  XMoveResizeWindow(dwm_x_display,
                    c->win,
                    c->x + 2 * dwm_x_screen_width,
                    c->y,
                    c->w,
                    c->h); /* some windows require this */
  dwm_set_x_window_state(c, NormalState);
  if (c->mon == dwm_this_screen)
    unfocus(dwm_this_screen->sel, 0);
  c->mon->sel = c;
  arrange(c->mon);
  XMapWindow(dwm_x_display, c->win);
  focus(NULL);
}

void mappingnotify(XEvent* e) {
  XMappingEvent* ev = &e->xmapping;

  XRefreshKeyboardMapping(ev);
  if (ev->request == MappingKeyboard)
    grabkeys();
}

void maprequest(XEvent* e) {
  static XWindowAttributes wa;
  XMapRequestEvent* ev = &e->xmaprequest;

  dwm_client_t* i;
  if ((i = dwm_find_systray_icon_window(ev->window))) {
    dwm_send_systray_icon_window_active(i->win);
    move_resize_bar(dwm_this_screen);
    dwm_update_systray();
  }

  if (!XGetWindowAttributes(dwm_x_display, ev->window, &wa))
    return;
  if (wa.override_redirect)
    return;
  if (!wintoclient(ev->window))
    manage(ev->window, &wa);
}

void monocle(dwm_monitor_t* m) {
  unsigned int n = 0;
  dwm_client_t* c;

  for (c = m->clients; c; c = c->next)
    if (ISVISIBLE(c))
      n++;
  if (n > 0) /* override layout symbol */
    snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
  for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
    resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void motionnotify(XEvent* e) {
  static dwm_monitor_t* mon = NULL;
  dwm_monitor_t* m;
  XMotionEvent* ev = &e->xmotion;

  if (ev->window != dwm_x_window)
    return;
  if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
    unfocus(dwm_this_screen->sel, 1);
    dwm_this_screen = m;
    focus(NULL);
  }
  mon = m;
}

void movemouse(const Arg* arg) {
  int x, y, ocx, ocy, nx, ny;
  dwm_client_t* c;
  dwm_monitor_t* m;
  XEvent ev;
  Time lasttime = 0;

  if (!(c = dwm_this_screen->sel))
    return;
  if (c->isfullscreen) /* no support moving fullscreen windows by mouse */
    return;
  restack(dwm_this_screen);
  ocx = c->x;
  ocy = c->y;
  if (XGrabPointer(dwm_x_display,
                   dwm_x_window,
                   False,
                   MOUSEMASK,
                   GrabModeAsync,
                   GrabModeAsync,
                   None,
                   cursor[CurMove]->cursor,
                   CurrentTime)
      != GrabSuccess)
    return;
  if (!getrootptr(&x, &y))
    return;
  do {
    XMaskEvent(dwm_x_display, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      drw_x_event_handlers[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      nx = ocx + (ev.xmotion.x - x);
      ny = ocy + (ev.xmotion.y - y);
      if (abs(dwm_this_screen->wx - nx) < snap)
        nx = dwm_this_screen->wx;
      else if (abs((dwm_this_screen->wx + dwm_this_screen->ww) - (nx + WIDTH(c))) < snap)
        nx = dwm_this_screen->wx + dwm_this_screen->ww - WIDTH(c);
      if (abs(dwm_this_screen->wy - ny) < snap)
        ny = dwm_this_screen->wy;
      else if (abs((dwm_this_screen->wy + dwm_this_screen->wh) - (ny + HEIGHT(c))) < snap)
        ny = dwm_this_screen->wy + dwm_this_screen->wh - HEIGHT(c);
      if (!c->isfloating && dwm_this_screen->lt[dwm_this_screen->sellt]->arrange
          && (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
        togglefloating(NULL);
      if (!dwm_this_screen->lt[dwm_this_screen->sellt]->arrange || c->isfloating)
        resize(c, nx, ny, c->w, c->h, 1);
      break;
    }
  } while (ev.type != ButtonRelease);
  XUngrabPointer(dwm_x_display, CurrentTime);
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != dwm_this_screen) {
    sendmon(c, m);
    dwm_this_screen = m;
    focus(NULL);
  }
}

dwm_client_t* nexttiled(dwm_client_t* c) {
  for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next)
    ;
  return c;
}

void pop(dwm_client_t* c) {
  detach(c);
  attach(c);
  focus(c);
  arrange(c->mon);
}

void propertynotify(XEvent* e) {
  dwm_client_t* c;
  Window trans;
  XPropertyEvent* ev = &e->xproperty;

  if ((c = dwm_find_systray_icon_window(ev->window))) {
    if (ev->atom == XA_WM_NORMAL_HINTS) {
      dwm_update_size_hints(c);
      dwm_update_systray_icon_geom(c, c->w, c->h);
    } else
      dwm_update_systray_icon_state(c, ev);
    move_resize_bar(dwm_this_screen);
    dwm_update_systray();
  }

  if ((ev->window == dwm_x_window) && (ev->atom == XA_WM_NAME))
    drawbar(dwm_this_screen);
  else if (ev->state == PropertyDelete)
    return; /* ignore */
  else if ((c = wintoclient(ev->window))) {
    switch (ev->atom) {
    default:
      break;
    case XA_WM_TRANSIENT_FOR:
      if (!c->isfloating && (XGetTransientForHint(dwm_x_display, c->win, &trans))
          && (c->isfloating = (wintoclient(trans)) != NULL))
        arrange(c->mon);
      break;
    case XA_WM_NORMAL_HINTS:
      dwm_update_size_hints(c);
      break;
    case XA_WM_HINTS:
      updatewmhints(c);
      drawbars();
      break;
    }
    if (ev->atom == XA_WM_NAME || ev->atom == dwm_x_net_atoms[NetWMName]) {
      updatetitle(c);
      if (c == c->mon->sel)
        drawbar(c->mon);
    }
    if (ev->atom == dwm_x_net_atoms[NetWMWindowType])
      updatewindowtype(c);
  }
}

void quit(const Arg* arg) { running = 0; }

dwm_monitor_t* recttomon(int x, int y, int w, int h) {
  dwm_monitor_t *m, *r = dwm_this_screen;
  int a, area = 0;

  for (m = dwm_screens; m; m = m->next)
    if ((a = INTERSECT(x, y, w, h, m)) > area) {
      area = a;
      r = m;
    }
  return r;
}

void resizerequest(XEvent* e) {
  XResizeRequestEvent* ev = &e->xresizerequest;
  dwm_client_t* i;

  if ((i = dwm_find_systray_icon_window(ev->window))) {
    dwm_update_systray_icon_geom(i, ev->width, ev->height);
    move_resize_bar(dwm_this_screen);
    dwm_update_systray();
  }
}

void move_resize_bar(dwm_monitor_t* m) {
  unsigned int w = m->ww;
  if (DWM_HAS_SYSTRAY && m == dwm_find_systray_screen(m))
    w -= dwm_calculate_systray_width();
  XMoveResizeWindow(dwm_x_display, m->barwin, m->wx, m->by, w, dwm_bar_height);
}

void resize(dwm_client_t* c, int x, int y, int w, int h, int interact) {
  if (dwm_apply_size_hints(c, &x, &y, &w, &h, interact))
    resizeclient(c, x, y, w, h);
}

void resizeclient(dwm_client_t* c, int x, int y, int w, int h) {
  XWindowChanges wc;

  c->oldx = c->x;
  c->x = wc.x = x;
  c->oldy = c->y;
  c->y = wc.y = y;
  c->oldw = c->w;
  c->w = wc.width = w;
  c->oldh = c->h;
  c->h = wc.height = h;
  wc.border_width = c->bw;
  XConfigureWindow(
    dwm_x_display, c->win, CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
  configure(c);
  XSync(dwm_x_display, False);
}

void resizemouse(const Arg* arg) {
  int ocx, ocy, nw, nh;
  dwm_client_t* c;
  dwm_monitor_t* m;
  XEvent ev;
  Time lasttime = 0;

  if (!(c = dwm_this_screen->sel))
    return;
  if (c->isfullscreen) /* no support resizing fullscreen windows by mouse */
    return;
  restack(dwm_this_screen);
  ocx = c->x;
  ocy = c->y;
  if (XGrabPointer(dwm_x_display,
                   dwm_x_window,
                   False,
                   MOUSEMASK,
                   GrabModeAsync,
                   GrabModeAsync,
                   None,
                   cursor[CurResize]->cursor,
                   CurrentTime)
      != GrabSuccess)
    return;
  XWarpPointer(
    dwm_x_display, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
  do {
    XMaskEvent(dwm_x_display, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      drw_x_event_handlers[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
      nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
      if (c->mon->wx + nw >= dwm_this_screen->wx
          && c->mon->wx + nw <= dwm_this_screen->wx + dwm_this_screen->ww
          && c->mon->wy + nh >= dwm_this_screen->wy
          && c->mon->wy + nh <= dwm_this_screen->wy + dwm_this_screen->wh) {
        if (!c->isfloating && dwm_this_screen->lt[dwm_this_screen->sellt]->arrange
            && (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
          togglefloating(NULL);
      }
      if (!dwm_this_screen->lt[dwm_this_screen->sellt]->arrange || c->isfloating)
        resize(c, c->x, c->y, nw, nh, 1);
      break;
    }
  } while (ev.type != ButtonRelease);
  XWarpPointer(
    dwm_x_display, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
  XUngrabPointer(dwm_x_display, CurrentTime);
  while (XCheckMaskEvent(dwm_x_display, EnterWindowMask, &ev))
    ;
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != dwm_this_screen) {
    sendmon(c, m);
    dwm_this_screen = m;
    focus(NULL);
  }
}

void restack(dwm_monitor_t* m) {
  dwm_client_t* c;
  XEvent ev;
  XWindowChanges wc;

  drawbar(m);
  if (!m->sel)
    return;
  if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
    XRaiseWindow(dwm_x_display, m->sel->win);
  if (m->lt[m->sellt]->arrange) {
    wc.stack_mode = Below;
    wc.sibling = m->barwin;
    for (c = m->stack; c; c = c->snext)
      if (!c->isfloating && ISVISIBLE(c)) {
        XConfigureWindow(dwm_x_display, c->win, CWSibling | CWStackMode, &wc);
        wc.sibling = c->win;
      }
  }
  XSync(dwm_x_display, False);
  while (XCheckMaskEvent(dwm_x_display, EnterWindowMask, &ev))
    ;
}

void run(void) {
  XEvent ev;
  /* main event loop */
  XSync(dwm_x_display, False);
  while (running && !XNextEvent(dwm_x_display, &ev))
    if (drw_x_event_handlers[ev.type])
      drw_x_event_handlers[ev.type](&ev); /* call handler */
}

void scan(void) {
  unsigned int i, num;
  Window d1, d2, *wins = NULL;
  XWindowAttributes wa;

  if (XQueryTree(dwm_x_display, dwm_x_window, &d1, &d2, &wins, &num)) {
    for (i = 0; i < num; i++) {
      if (!XGetWindowAttributes(dwm_x_display, wins[i], &wa) || wa.override_redirect
          || XGetTransientForHint(dwm_x_display, wins[i], &d1))
        continue;
      if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
        manage(wins[i], &wa);
    }
    for (i = 0; i < num; i++) { /* now the transients */
      if (!XGetWindowAttributes(dwm_x_display, wins[i], &wa))
        continue;
      if (XGetTransientForHint(dwm_x_display, wins[i], &d1)
          && (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
        manage(wins[i], &wa);
    }
    if (wins)
      XFree(wins);
  }
}

void sendmon(dwm_client_t* c, dwm_monitor_t* m) {
  if (c->mon == m)
    return;
  unfocus(c, 1);
  detach(c);
  detachstack(c);
  c->mon = m;
  c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
  attach(c);
  attachstack(c);
  focus(NULL);
  arrange(NULL);
}

void setfocus(dwm_client_t* c) {
  if (!c->neverfocus) {
    XSetInputFocus(dwm_x_display, c->win, RevertToPointerRoot, CurrentTime);
    XChangeProperty(dwm_x_display,
                    dwm_x_window,
                    dwm_x_net_atoms[NetActiveWindow],
                    XA_WINDOW,
                    32,
                    PropModeReplace,
                    (unsigned char*)&(c->win),
                    1);
  }
  dwm_send_x_event(c->win,
                   dwm_x_wm_atoms[WMTakeFocus],
                   NoEventMask,
                   dwm_x_wm_atoms[WMTakeFocus],
                   CurrentTime,
                   0,
                   0,
                   0);
}

void setfullscreen(dwm_client_t* c, int fullscreen) {
  if (fullscreen && !c->isfullscreen) {
    XChangeProperty(dwm_x_display,
                    c->win,
                    dwm_x_net_atoms[NetWMState],
                    XA_ATOM,
                    32,
                    PropModeReplace,
                    (unsigned char*)&dwm_x_net_atoms[NetWMFullscreen],
                    1);
    c->isfullscreen = 1;
    c->oldstate = c->isfloating;
    c->oldbw = c->bw;
    c->bw = 0;
    c->isfloating = 1;
    resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
    XRaiseWindow(dwm_x_display, c->win);
  } else if (!fullscreen && c->isfullscreen) {
    XChangeProperty(dwm_x_display,
                    c->win,
                    dwm_x_net_atoms[NetWMState],
                    XA_ATOM,
                    32,
                    PropModeReplace,
                    (unsigned char*)0,
                    0);
    c->isfullscreen = 0;
    c->isfloating = c->oldstate;
    c->bw = c->oldbw;
    c->x = c->oldx;
    c->y = c->oldy;
    c->w = c->oldw;
    c->h = c->oldh;
    resizeclient(c, c->x, c->y, c->w, c->h);
    arrange(c->mon);
  }
}

void setlayout(const Arg* arg) {
  if (!arg || !arg->v || arg->v != dwm_this_screen->lt[dwm_this_screen->sellt])
    dwm_this_screen->sellt ^= 1;
  if (arg && arg->v)
    dwm_this_screen->lt[dwm_this_screen->sellt] = (dwm_layout_t*)arg->v;
  strncpy(dwm_this_screen->ltsymbol,
          dwm_this_screen->lt[dwm_this_screen->sellt]->symbol,
          sizeof dwm_this_screen->ltsymbol);
  if (dwm_this_screen->sel)
    arrange(dwm_this_screen);
  else
    drawbar(dwm_this_screen);
}

/* arg > 1.0 will set mfact absolutely */
void setmfact(const Arg* arg) {
  float f;

  if (!arg || !dwm_this_screen->lt[dwm_this_screen->sellt]->arrange)
    return;
  f = arg->f < 1.0 ? arg->f + dwm_this_screen->mfact : arg->f - 1.0;
  if (f < 0.1 || f > 0.9)
    return;
  dwm_this_screen->mfact = f;
  arrange(dwm_this_screen);
}

void setup(void) {
  int i;
  XSetWindowAttributes wa;
  Atom utf8string;

  /* clean up any zombies immediately */
  sigchld(0);

  /* init screen */
  dwm_x_screen = DefaultScreen(dwm_x_display);
  dwm_x_screen_width = DisplayWidth(dwm_x_display, dwm_x_screen);
  dwm_x_screen_height = DisplayHeight(dwm_x_display, dwm_x_screen);
  dwm_x_window = RootWindow(dwm_x_display, dwm_x_screen);
  dwm_init_dwm_drw(dwm_drw,
                   dwm_x_display,
                   dwm_x_screen,
                   dwm_x_window,
                   dwm_x_screen_width,
                   dwm_x_screen_height);
  if (!dwm_drw_fontset_create(dwm_drw, fonts, LENGTH(fonts)))
    die("no fonts could be loaded.");
  lrpad = dwm_drw->fonts->h;
  dwm_bar_height = dwm_drw->fonts->h + 2;
  updategeom();
  /* init atoms */
  utf8string = XInternAtom(dwm_x_display, "UTF8_STRING", False);
  dwm_x_wm_atoms[WMProtocols] = XInternAtom(dwm_x_display, "WM_PROTOCOLS", False);
  dwm_x_wm_atoms[WMDelete] = XInternAtom(dwm_x_display, "WM_DELETE_WINDOW", False);
  dwm_x_wm_atoms[WMState] = XInternAtom(dwm_x_display, "WM_STATE", False);
  dwm_x_wm_atoms[WMTakeFocus] = XInternAtom(dwm_x_display, "WM_TAKE_FOCUS", False);
  dwm_x_net_atoms[NetActiveWindow]
    = XInternAtom(dwm_x_display, "_NET_ACTIVE_WINDOW", False);
  dwm_x_net_atoms[NetSupported] = XInternAtom(dwm_x_display, "_NET_SUPPORTED", False);
  dwm_x_net_atoms[NetSystemTray]
    = XInternAtom(dwm_x_display, "_NET_SYSTEM_TRAY_S0", False);
  dwm_x_net_atoms[NetSystemTrayOP]
    = XInternAtom(dwm_x_display, "_NET_SYSTEM_TRAY_OPCODE", False);
  dwm_x_net_atoms[NetSystemTrayOrientation]
    = XInternAtom(dwm_x_display, "_NET_SYSTEM_TRAY_ORIENTATION", False);
  dwm_x_net_atoms[NetWMName] = XInternAtom(dwm_x_display, "_NET_WM_NAME", False);
  dwm_x_net_atoms[NetWMState] = XInternAtom(dwm_x_display, "_NET_WM_STATE", False);
  dwm_x_net_atoms[NetWMCheck]
    = XInternAtom(dwm_x_display, "_NET_SUPPORTING_WM_CHECK", False);
  dwm_x_net_atoms[NetWMFullscreen]
    = XInternAtom(dwm_x_display, "_NET_WM_STATE_FULLSCREEN", False);
  dwm_x_net_atoms[NetWMWindowType]
    = XInternAtom(dwm_x_display, "_NET_WM_WINDOW_TYPE", False);
  dwm_x_net_atoms[NetWMWindowTypeDialog]
    = XInternAtom(dwm_x_display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  dwm_x_net_atoms[NetClientList] = XInternAtom(dwm_x_display, "_NET_CLIENT_LIST", False);
  dwm_x_atoms[Manager] = XInternAtom(dwm_x_display, "MANAGER", False);
  dwm_x_atoms[Xembed] = XInternAtom(dwm_x_display, "_XEMBED", False);
  dwm_x_atoms[XembedInfo] = XInternAtom(dwm_x_display, "_XEMBED_INFO", False);
  /* init cursors */
  cursor[CurNormal] = dwm_drw_cur_create(dwm_drw, XC_left_ptr);
  cursor[CurResize] = dwm_drw_cur_create(dwm_drw, XC_sizing);
  cursor[CurMove] = dwm_drw_cur_create(dwm_drw, XC_fleur);
  /* init appearance */
  dwm_color_schemes = ecalloc(LENGTH(colors), sizeof(XftColor*));
  for (i = 0; i < LENGTH(colors); i++)
    dwm_color_schemes[i] = dwm_drw_scm_create(dwm_drw, colors[i], 3);
  /* init system tray */
  dwm_create_systray();
  dwm_update_systray();
  /* init bars */
  updatebars();
  drawbar(dwm_this_screen);
  /* supporting window for NetWMCheck */
  wmcheckwin = XCreateSimpleWindow(dwm_x_display, dwm_x_window, 0, 0, 1, 1, 0, 0, 0);
  XChangeProperty(dwm_x_display,
                  wmcheckwin,
                  dwm_x_net_atoms[NetWMCheck],
                  XA_WINDOW,
                  32,
                  PropModeReplace,
                  (unsigned char*)&wmcheckwin,
                  1);
  XChangeProperty(dwm_x_display,
                  wmcheckwin,
                  dwm_x_net_atoms[NetWMName],
                  utf8string,
                  8,
                  PropModeReplace,
                  (unsigned char*)"dwm",
                  3);
  XChangeProperty(dwm_x_display,
                  dwm_x_window,
                  dwm_x_net_atoms[NetWMCheck],
                  XA_WINDOW,
                  32,
                  PropModeReplace,
                  (unsigned char*)&wmcheckwin,
                  1);
  /* EWMH support per view */
  XChangeProperty(dwm_x_display,
                  dwm_x_window,
                  dwm_x_net_atoms[NetSupported],
                  XA_ATOM,
                  32,
                  PropModeReplace,
                  (unsigned char*)dwm_x_net_atoms,
                  _NetLast);
  XDeleteProperty(dwm_x_display, dwm_x_window, dwm_x_net_atoms[NetClientList]);
  /* select events */
  wa.cursor = cursor[CurNormal]->cursor;
  wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask
    | PointerMotionMask | EnterWindowMask | LeaveWindowMask | StructureNotifyMask
    | PropertyChangeMask;
  XChangeWindowAttributes(dwm_x_display, dwm_x_window, CWEventMask | CWCursor, &wa);
  XSelectInput(dwm_x_display, dwm_x_window, wa.event_mask);
  grabkeys();
  focus(NULL);
}

void seturgent(dwm_client_t* c, int urg) {
  XWMHints* wmh;

  c->isurgent = urg;
  if (!(wmh = XGetWMHints(dwm_x_display, c->win)))
    return;
  wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
  XSetWMHints(dwm_x_display, c->win, wmh);
  XFree(wmh);
}

void showhide(dwm_client_t* c) {
  if (!c)
    return;
  if (ISVISIBLE(c)) {
    /* show clients top down */
    XMoveWindow(dwm_x_display, c->win, c->x, c->y);
    if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
      resize(c, c->x, c->y, c->w, c->h, 0);
    showhide(c->snext);
  } else {
    /* hide clients bottom up */
    showhide(c->snext);
    XMoveWindow(dwm_x_display, c->win, WIDTH(c) * -2, c->y);
  }
}

void sigchld(int unused) {
  if (signal(SIGCHLD, sigchld) == SIG_ERR)
    die("can't install SIGCHLD handler:");
  while (0 < waitpid(-1, NULL, WNOHANG))
    ;
}

void spawn(const Arg* arg) {
  if (arg->v == dmenucmd)
    dmenumon[0] = '0' + dwm_this_screen->num;
  if (fork() == 0) {
    if (dwm_x_display)
      close(ConnectionNumber(dwm_x_display));
    setsid();
    execvp(((char**)arg->v)[0], (char**)arg->v);
    fprintf(stderr, "dwm: execvp %s", ((char**)arg->v)[0]);
    perror(" failed");
    exit(EXIT_SUCCESS);
  }
}

void tag(const Arg* arg) {
  if (dwm_this_screen->sel && arg->ui & TAGMASK) {
    dwm_this_screen->sel->tags = arg->ui & TAGMASK;
    focus(NULL);
    arrange(dwm_this_screen);
  }
}

void tagmon(const Arg* arg) {
  if (!dwm_this_screen->sel || !dwm_screens->next)
    return;
  sendmon(dwm_this_screen->sel, dirtomon(arg->i));
}

void tile(dwm_monitor_t* m) {
  unsigned int i, n, h, mw, my, ty;
  dwm_client_t* c;

  for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++)
    ;
  if (n == 0)
    return;

  if (n > m->nmaster)
    mw = m->nmaster ? m->ww * m->mfact : 0;
  else
    mw = m->ww;
  for (i = my = ty = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
    if (i < m->nmaster) {
      h = (m->wh - my) / (MIN(n, m->nmaster) - i);
      resize(c, m->wx, m->wy + my, mw - (2 * c->bw), h - (2 * c->bw), 0);
      my += HEIGHT(c);
    } else {
      h = (m->wh - ty) / (n - i);
      resize(c, m->wx + mw, m->wy + ty, m->ww - mw - (2 * c->bw), h - (2 * c->bw), 0);
      ty += HEIGHT(c);
    }
}

void togglebar(const Arg* arg) {
  dwm_this_screen->showbar = !dwm_this_screen->showbar;
  updatebarpos(dwm_this_screen);
  move_resize_bar(dwm_this_screen);
  dwm_toggle_systray();
  arrange(dwm_this_screen);
}

void togglefloating(const Arg* arg) {
  if (!dwm_this_screen->sel)
    return;
  if (dwm_this_screen->sel->isfullscreen) /* no support for fullscreen windows */
    return;
  dwm_this_screen->sel->isfloating
    = !dwm_this_screen->sel->isfloating || dwm_this_screen->sel->isfixed;
  if (dwm_this_screen->sel->isfloating)
    resize(dwm_this_screen->sel,
           dwm_this_screen->sel->x,
           dwm_this_screen->sel->y,
           dwm_this_screen->sel->w,
           dwm_this_screen->sel->h,
           0);
  arrange(dwm_this_screen);
}

void toggletag(const Arg* arg) {
  unsigned int newtags;

  if (!dwm_this_screen->sel)
    return;
  newtags = dwm_this_screen->sel->tags ^ (arg->ui & TAGMASK);
  if (newtags) {
    dwm_this_screen->sel->tags = newtags;
    focus(NULL);
    arrange(dwm_this_screen);
  }
}

void toggleview(const Arg* arg) {
  unsigned int newtagset
    = dwm_this_screen->tagset[dwm_this_screen->seltags] ^ (arg->ui & TAGMASK);

  if (newtagset) {
    dwm_this_screen->tagset[dwm_this_screen->seltags] = newtagset;
    focus(NULL);
    arrange(dwm_this_screen);
  }
}

void unfocus(dwm_client_t* c, int setfocus) {
  if (!c)
    return;
  grabbuttons(c, 0);
  XSetWindowBorder(
    dwm_x_display, c->win, dwm_color_schemes[DwmNormalScheme][DwmBorderColor].pixel);
  if (setfocus) {
    XSetInputFocus(dwm_x_display, dwm_x_window, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dwm_x_display, dwm_x_window, dwm_x_net_atoms[NetActiveWindow]);
  }
}

void unmanage(dwm_client_t* c, int destroyed) {
  dwm_monitor_t* m = c->mon;
  XWindowChanges wc;

  detach(c);
  detachstack(c);
  if (!destroyed) {
    wc.border_width = c->oldbw;
    XGrabServer(dwm_x_display); /* avoid race conditions */
    XSetErrorHandler(xerrordummy);
    XConfigureWindow(dwm_x_display, c->win, CWBorderWidth, &wc); /* restore border */
    XUngrabButton(dwm_x_display, AnyButton, AnyModifier, c->win);
    dwm_set_x_window_state(c, WithdrawnState);
    XSync(dwm_x_display, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dwm_x_display);
  }
  free(c);
  focus(NULL);
  updateclientlist();
  arrange(m);
}

void unmapnotify(XEvent* e) {
  dwm_client_t* c;
  XUnmapEvent* ev = &e->xunmap;

  if ((c = wintoclient(ev->window))) {
    if (ev->send_event)
      dwm_set_x_window_state(c, WithdrawnState);
    else
      unmanage(c, 0);
  } else if ((c = dwm_find_systray_icon_window(ev->window))) {
    dwm_remove_systray_icon(c);
    move_resize_bar(dwm_this_screen);
    dwm_update_systray();
  }
}

void updatebars(void) {
  dwm_monitor_t* m;
  XSetWindowAttributes wa = {.override_redirect = True,
                             .background_pixmap = ParentRelative,
                             .event_mask = ButtonPressMask | ExposureMask};
  XClassHint ch = {"dwm", "dwm"};
  for (m = dwm_screens; m; m = m->next) {
    if (m->barwin)
      continue;
    unsigned int w = m->ww;
    if (DWM_HAS_SYSTRAY && m == dwm_find_systray_screen(m))
      w -= dwm_calculate_systray_width();
    m->barwin = XCreateWindow(dwm_x_display,
                              dwm_x_window,
                              m->wx,
                              m->by,
                              w,
                              dwm_bar_height,
                              0,
                              DefaultDepth(dwm_x_display, dwm_x_screen),
                              CopyFromParent,
                              DefaultVisual(dwm_x_display, dwm_x_screen),
                              CWOverrideRedirect | CWBackPixmap | CWEventMask,
                              &wa);
    XDefineCursor(dwm_x_display, m->barwin, cursor[CurNormal]->cursor);
    dwm_raise_systray(m);
    XMapRaised(dwm_x_display, m->barwin);
    XSetClassHint(dwm_x_display, m->barwin, &ch);
  }
}

void updatebarpos(dwm_monitor_t* m) {
  m->wy = m->my;
  m->wh = m->mh;
  if (m->showbar) {
    m->wh -= dwm_bar_height;
    m->by = m->topbar ? m->wy : m->wy + m->wh;
    m->wy = m->topbar ? m->wy + dwm_bar_height : m->wy;
  } else
    m->by = -dwm_bar_height;
}

void updateclientlist() {
  dwm_client_t* c;
  dwm_monitor_t* m;

  XDeleteProperty(dwm_x_display, dwm_x_window, dwm_x_net_atoms[NetClientList]);
  for (m = dwm_screens; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      XChangeProperty(dwm_x_display,
                      dwm_x_window,
                      dwm_x_net_atoms[NetClientList],
                      XA_WINDOW,
                      32,
                      PropModeAppend,
                      (unsigned char*)&(c->win),
                      1);
}

int updategeom(void) {
  int dirty = 0;

#ifdef XINERAMA
  if (XineramaIsActive(dwm_x_display)) {
    int i, j, n, nn;
    dwm_client_t* c;
    dwm_monitor_t* m;
    XineramaScreenInfo* info = XineramaQueryScreens(dwm_x_display, &nn);
    XineramaScreenInfo* unique = NULL;

    for (n = 0, m = dwm_screens; m; m = m->next, n++)
      ;
    /* only consider unique geometries as separate screens */
    unique = ecalloc(nn, sizeof(XineramaScreenInfo));
    for (i = 0, j = 0; i < nn; i++)
      if (isuniquegeom(unique, j, &info[i]))
        memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
    XFree(info);
    nn = j;
    if (n <= nn) { /* new monitors available */
      for (i = 0; i < (nn - n); i++) {
        for (m = dwm_screens; m && m->next; m = m->next)
          ;
        if (m)
          m->next = createmon();
        else
          dwm_screens = createmon();
      }
      for (i = 0, m = dwm_screens; i < nn && m; m = m->next, i++)
        if (i >= n || unique[i].x_org != m->mx || unique[i].y_org != m->my
            || unique[i].width != m->mw
            || unique[i].height != m->mh) {
          dirty = 1;
          m->num = i;
          m->mx = m->wx = unique[i].x_org;
          m->my = m->wy = unique[i].y_org;
          m->mw = m->ww = unique[i].width;
          m->mh = m->wh = unique[i].height;
          updatebarpos(m);
        }
    } else { /* less monitors available nn < n */
      for (i = nn; i < n; i++) {
        for (m = dwm_screens; m && m->next; m = m->next)
          ;
        while ((c = m->clients)) {
          dirty = 1;
          m->clients = c->next;
          detachstack(c);
          c->mon = dwm_screens;
          attach(c);
          attachstack(c);
        }
        if (m == dwm_this_screen)
          dwm_this_screen = dwm_screens;
        cleanupmon(m);
      }
    }
    free(unique);
  } else
#endif /* XINERAMA */
  { /* default monitor setup */
    if (!dwm_screens)
      dwm_screens = createmon();
    if (dwm_screens->mw != dwm_x_screen_width || dwm_screens->mh != dwm_x_screen_height) {
      dirty = 1;
      dwm_screens->mw = dwm_screens->ww = dwm_x_screen_width;
      dwm_screens->mh = dwm_screens->wh = dwm_x_screen_height;
      updatebarpos(dwm_screens);
    }
  }
  if (dirty) {
    dwm_this_screen = dwm_screens;
    dwm_this_screen = wintomon(dwm_x_window);
  }
  return dirty;
}

void updatenumlockmask(void) {
  unsigned int i, j;
  XModifierKeymap* modmap;

  numlockmask = 0;
  modmap = XGetModifierMapping(dwm_x_display);
  for (i = 0; i < 8; i++)
    for (j = 0; j < modmap->max_keypermod; j++)
      if (modmap->modifiermap[i * modmap->max_keypermod + j]
          == XKeysymToKeycode(dwm_x_display, XK_Num_Lock))
        numlockmask = (1 << i);
  XFreeModifiermap(modmap);
}

void updatetitle(dwm_client_t* c) {
  if (!gettextprop(c->win, dwm_x_net_atoms[NetWMName], c->name, sizeof c->name))
    gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
  if (c->name[0] == '\0') /* hack to mark broken clients */
    strcpy(c->name, broken);
}

void updatewindowtype(dwm_client_t* c) {
  Atom state = dwm_get_x_atom_property(c, dwm_x_net_atoms[NetWMState]);
  Atom wtype = dwm_get_x_atom_property(c, dwm_x_net_atoms[NetWMWindowType]);

  if (state == dwm_x_net_atoms[NetWMFullscreen])
    setfullscreen(c, 1);
  if (wtype == dwm_x_net_atoms[NetWMWindowTypeDialog])
    c->isfloating = 1;
}

void updatewmhints(dwm_client_t* c) {
  XWMHints* wmh;

  if ((wmh = XGetWMHints(dwm_x_display, c->win))) {
    if (c == dwm_this_screen->sel && wmh->flags & XUrgencyHint) {
      wmh->flags &= ~XUrgencyHint;
      XSetWMHints(dwm_x_display, c->win, wmh);
    } else
      c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
    if (wmh->flags & InputHint)
      c->neverfocus = !wmh->input;
    else
      c->neverfocus = 0;
    XFree(wmh);
  }
}

void view(const Arg* arg) {
  if ((arg->ui & TAGMASK) == dwm_this_screen->tagset[dwm_this_screen->seltags])
    return;
  dwm_this_screen->seltags ^= 1; /* toggle sel tagset */
  if (arg->ui & TAGMASK)
    dwm_this_screen->tagset[dwm_this_screen->seltags] = arg->ui & TAGMASK;
  focus(NULL);
  arrange(dwm_this_screen);
}

dwm_client_t* wintoclient(Window w) {
  dwm_client_t* c;
  dwm_monitor_t* m;

  for (m = dwm_screens; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      if (c->win == w)
        return c;
  return NULL;
}

dwm_monitor_t* wintomon(Window w) {
  int x, y;
  dwm_client_t* c;
  dwm_monitor_t* m;

  if (w == dwm_x_window && getrootptr(&x, &y))
    return recttomon(x, y, 1, 1);
  for (m = dwm_screens; m; m = m->next)
    if (w == m->barwin)
      return m;
  if ((c = wintoclient(w)))
    return c->mon;
  return dwm_this_screen;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int xerror(Display* dwm_x_display, XErrorEvent* ee) {
  if (ee->error_code == BadWindow
      || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
      || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
      || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
      || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
      || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
      || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
      || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
      || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
    return 0;
  fprintf(stderr,
          "dwm: fatal error: request code=%d, error code=%d\n",
          ee->request_code,
          ee->error_code);
  return xerrorxlib(dwm_x_display, ee); /* may call exit */
}

int xerrordummy(Display* dwm_x_display, XErrorEvent* ee) { return 0; }

/* Startup Error handler to check if another window manager
 * is already running. */
int xerrorstart(Display* dwm_x_display, XErrorEvent* ee) {
  die("dwm: another window manager is already running");
  return -1;
}

void zoom(const Arg* arg) {
  dwm_client_t* c = dwm_this_screen->sel;

  if (!dwm_this_screen->lt[dwm_this_screen->sellt]->arrange
      || (dwm_this_screen->sel && dwm_this_screen->sel->isfloating))
    return;
  if (c == nexttiled(dwm_this_screen->clients))
    if (!c || !(c = nexttiled(c->next)))
      return;
  pop(c);
}

void movestack(const Arg* arg) {
  dwm_client_t *c = NULL, *p = NULL, *pc = NULL, *i;

  if (arg->i > 0) {
    /* find the client after dwm_this_screen->sel */
    for (c = dwm_this_screen->sel->next; c && (!ISVISIBLE(c) || c->isfloating);
         c = c->next)
      ;
    if (!c)
      for (c = dwm_this_screen->clients; c && (!ISVISIBLE(c) || c->isfloating);
           c = c->next)
        ;

  } else {
    /* find the client before dwm_this_screen->sel */
    for (i = dwm_this_screen->clients; i != dwm_this_screen->sel; i = i->next)
      if (ISVISIBLE(i) && !i->isfloating)
        c = i;
    if (!c)
      for (; i; i = i->next)
        if (ISVISIBLE(i) && !i->isfloating)
          c = i;
  }
  /* find the client before dwm_this_screen->sel and c */
  for (i = dwm_this_screen->clients; i && (!p || !pc); i = i->next) {
    if (i->next == dwm_this_screen->sel)
      p = i;
    if (i->next == c)
      pc = i;
  }

  /* swap c and dwm_this_screen->sel dwm_this_screen->clients in the dwm_this_screen->clients list */
  if (c && c != dwm_this_screen->sel) {
    dwm_client_t* temp = dwm_this_screen->sel->next == c ? dwm_this_screen->sel
                                                         : dwm_this_screen->sel->next;
    dwm_this_screen->sel->next = c->next == dwm_this_screen->sel ? c : c->next;
    c->next = temp;

    if (p && p != c)
      p->next = c;
    if (pc && pc != dwm_this_screen->sel)
      pc->next = dwm_this_screen->sel;

    if (dwm_this_screen->sel == dwm_this_screen->clients)
      dwm_this_screen->clients = c;
    else if (c == dwm_this_screen->clients)
      dwm_this_screen->clients = dwm_this_screen->sel;

    arrange(dwm_this_screen);
  }
}

static void startup() {
  size_t path_spare_len = 1024;
  char path[1024] = {0};
  char const* file_path_suffix = ".config/dwm/startup.sh";
  size_t const file_path_suffix_len = strlen(file_path_suffix);
  path_spare_len -= file_path_suffix_len + 1;
  char const* home_env = getenv("HOME");
  size_t len = 0;
  if (home_env) {
    len = strlen(home_env);
    if (len > path_spare_len)
      return;
    strncpy(path, home_env, len);
    path_spare_len -= len;
    if (path[len - 1] != '/') {
      if (path_spare_len < 1)
        return;
      path[len] = '/';
      ++len;
    }
  } else {
    path[len] = '/';
    ++len;
  }
  strncpy(path + len, file_path_suffix, file_path_suffix_len);

  FILE* f = fopen(path, "r");
  if (!f)
    return;
  fclose(f);
  char const* cmd[] = {"/bin/bash", path, NULL};
  Arg const arg = {.v = cmd};
  spawn(&arg);
}

void togglescratch(const Arg* arg) {
  if (dwm_this_screen->scratchpadpid)
    return;

  if (dwm_this_screen->scratchpad) {
    unsigned int newtagset
      = dwm_this_screen->tagset[dwm_this_screen->seltags] ^ scratchtag;
    if (newtagset) {
      dwm_this_screen->tagset[dwm_this_screen->seltags] = newtagset;
      focus(NULL);
      arrange(dwm_this_screen);
    }
    if (ISVISIBLE(dwm_this_screen->scratchpad)) {
      focus(dwm_this_screen->scratchpad);
      restack(dwm_this_screen);
    }
  } else {
    __pid_t pid = fork();
    if (pid == 0) {
      if (dwm_x_display)
        close(ConnectionNumber(dwm_x_display));
      setsid();
      execvp(((char**)arg->v)[0], (char**)arg->v);
      fprintf(stderr, "dwm: execvp %s", ((char**)arg->v)[0]);
      perror(" failed");
      exit(EXIT_SUCCESS);
    } else {
      dwm_this_screen->scratchpadpid = pid;
    }
  }
}

int main(int argc, char* argv[]) {
  if (argc == 2 && !strcmp("-v", argv[1]))
    die("dwm-" VERSION);
  else if (argc != 1)
    die("usage: dwm [-v]");
  if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
    fputs("warning: no locale support\n", stderr);
  if (!(dwm_x_display = XOpenDisplay(NULL)))
    die("dwm: cannot open display");
  checkotherwm();
  setup();
#ifdef __OpenBSD__
  if (pledge("stdio rpath proc exec", NULL) == -1)
    die("pledge");
#endif /* __OpenBSD__ */
  scan();
  startup();
  run();
  cleanup();
  XCloseDisplay(dwm_x_display);
  return EXIT_SUCCESS;
}