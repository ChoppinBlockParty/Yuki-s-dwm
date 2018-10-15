#pragma once

#include <X11/Xlib.h>

typedef struct dwm_screen_s dwm_screen_t;
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
  dwm_screen_t* mon;
  Window win;
} dwm_client_t;

typedef struct {
  const char* symbol;
  void (*arrange)(dwm_screen_t*);
} dwm_layout_t;

typedef struct dwm_screen_s {
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
  dwm_screen_t* next;
  Window barwin;
  const dwm_layout_t* lt[2];
} dwm_screen_t;

extern Display* dwm_x_display;
extern Window dwm_x_window;
extern dwm_screen_t* dwm_screens;
extern dwm_screen_t* dwm_this_screen;
