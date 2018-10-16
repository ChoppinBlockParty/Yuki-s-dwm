#pragma once

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>

#define WIDTH(X) ((X)->w + 2 * (X)->bw)
#define HEIGHT(X) ((X)->h + 2 * (X)->bw)

typedef struct dwm_monitor_s dwm_monitor_t;

typedef struct dwm_client_s dwm_client_t;

typedef struct dwm_client_s {
  char name[256];
  float mina, maxa;
  int x, y, w, h;
  int oldx, oldy, oldw, oldh;
  int basew, baseh, incw, inch, maxw, maxh, minw, minh;
  int bw, oldbw;
  unsigned int tags;
  int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
  dwm_client_t* next;
  dwm_client_t* snext;
  dwm_monitor_t* mon;
  Window win;
} dwm_client_t;

typedef struct {
  const char* symbol;
  void (*arrange)(dwm_monitor_t*);
} dwm_layout_t;

typedef struct dwm_monitor_s {
  char ltsymbol[16];
  float mfact;
  int nmaster;
  int num;
  int by; /* bar geometry */
  int mx, my, mw, mh; /* screen size */
  int wx, wy, ww, wh; /* window area  */
  unsigned int seltags;
  unsigned int sellt;
  unsigned int tagset[2];
  int showbar;
  int topbar;
  dwm_client_t* clients;
  dwm_client_t* sel;
  dwm_client_t* stack;
  dwm_client_t* scratchpad;
  unsigned long scratchpadpid;
  dwm_monitor_t* next;
  Window barwin;
  const dwm_layout_t* lt[2];
} dwm_monitor_t;

// EWMH atoms
enum {
  NetSupported,
  NetSystemTray,
  NetSystemTrayOP,
  NetSystemTrayOrientation,
  NetWMName,
  NetWMState,
  NetWMCheck,
  NetWMFullscreen,
  NetActiveWindow,
  NetWMWindowType,
  NetWMWindowTypeDialog,
  NetClientList,
  _NetLast
};
// Xembed atoms
enum { Manager, Xembed, XembedInfo, _XLast };
// default atoms
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, _WMLast };

// https://unix.stackexchange.com/questions/367732/what-are-display-and-screen-with-regard-to-0-0
// In X11 terminology.
//
// Display: at least one screen, a keyboard, and a pointing device
// (often a mouse).
//
// Screen: What everyone else calls a display, monitor, or screen, but
// could be virtual, e.g. a region of a monitor (window).
//
// Both screens and windows are addressable via the DISPLAY
// environment variable, and some other means. An application can
// choose which display.screen to map a window to. But it is not
// possible to move a window to another screen, without the
// application un-mapping and re-mapping it.
//
// Monitor: This is (I think), a newer idea. Each screen can be made
// up of monitors. Generally application don't know about monitors,
// except the window manager. The window manager can freely move
// windows between monitors, and even overlap. All monitors are mapped
// as a single rectangular screen. But the window manager knows where
// monitors start and end, and can full-screen to just one, or detect
// monitor edge gestures. (I think a monitor is probably no more that
// a set of hints that the window manager uses). If your window
// manager is not monitor aware, then windows will full-screen over
// the whole screen.
//
// Screens are not used much these days, at least not for interactive
// desktops, if using a window manager that supports monitors. However
// screens would be useful, when the application should be in charge,
// as opposed to the window manager. Though this does not seem to be
// necessary: Open-office presents, knows of, and uses monitors when
// presenting.

extern Display* dwm_x_display;
extern int dwm_x_screen;
extern int dwm_x_screen_width;
extern int dwm_x_screen_height;
extern Window dwm_x_window;

typedef struct dwm_drw_s dwm_drw_t;
extern dwm_drw_t* dwm_drw;

extern dwm_monitor_t* dwm_screens;
extern dwm_monitor_t* dwm_this_screen;
extern int dwm_bar_height;
extern XftColor** dwm_color_schemes;
extern Atom dwm_x_wm_atoms[_WMLast];
extern Atom dwm_x_net_atoms[_NetLast];
extern Atom dwm_x_atoms[_XLast];
