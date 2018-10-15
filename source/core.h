#pragma once

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "dwm_global.h"

enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel }; /* color schemes */
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
}; /* EWMH atoms */
enum { Manager, Xembed, XembedInfo, XLast }; /* Xembed atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum {
  ClkTagBar,
  ClkLtSymbol,
  ClkStatusText,
  ClkWinTitle,
  ClkClientWin,
  ClkRootWin,
  _ClkLast
}; /* clicks */

typedef union {
  int i;
  unsigned int ui;
  float f;
  const void* v;
} Arg;

typedef struct {
  unsigned int click;
  unsigned int mask;
  unsigned int button;
  void (*func)(const Arg* arg);
  const Arg arg;
} Button;


typedef struct {
  unsigned int mod;
  KeySym keysym;
  void (*func)(const Arg*);
  const Arg arg;
} Key;

typedef struct {
  const char* class;
  const char* instance;
  const char* title;
  unsigned int tags;
  int isfloating;
  int monitor;
} Rule;

typedef struct {
  Window win;
  dwm_client_t* icons;
} Systray;

static void applyrules(dwm_client_t* c);
static int applysizehints(dwm_client_t* c, int* x, int* y, int* w, int* h, int interact);
static void arrange(dwm_screen_t* m);
static void arrangemon(dwm_screen_t* m);
static void attach(dwm_client_t* c);
static void attachstack(dwm_client_t* c);
static void buttonpress(XEvent* e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(dwm_screen_t* mon);
static void clientmessage(XEvent* e);
static void configure(dwm_client_t* c);
static void configurenotify(XEvent* e);
static void configurerequest(XEvent* e);
static dwm_screen_t* createmon(void);
static void destroynotify(XEvent* e);
static void detach(dwm_client_t* c);
static void detachstack(dwm_client_t* c);
static dwm_screen_t* dirtomon(int dir);
static void drawbar(dwm_screen_t* m);
static void drawbars(void);
static void enternotify(XEvent* e);
static void expose(XEvent* e);
static void focus(dwm_client_t* c);
static void focusin(XEvent* e);
static void focusmon(const Arg* arg);
static void focusstack(const Arg* arg);
static int getrootptr(int* x, int* y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char* text, unsigned int size);
static void grabbuttons(dwm_client_t* c, int focused);
static void grabkeys(void);
static void incnmaster(const Arg* arg);
static void keypress(XEvent* e);
static void killclient(const Arg* arg);
static void manage(Window w, XWindowAttributes* wa);
static void mappingnotify(XEvent* e);
static void maprequest(XEvent* e);
static void monocle(dwm_screen_t* m);
static void motionnotify(XEvent* e);
static void movemouse(const Arg* arg);
static dwm_client_t* nexttiled(dwm_client_t* c);
static void pop(dwm_client_t*);
static void propertynotify(XEvent* e);
static void quit(const Arg* arg);
static dwm_screen_t* recttomon(int x, int y, int w, int h);
static void resizerequest(XEvent* e);
static void resizebarwin(dwm_screen_t* m);
static void resize(dwm_client_t* c, int x, int y, int w, int h, int interact);
static void resizeclient(dwm_client_t* c, int x, int y, int w, int h);
static void resizemouse(const Arg* arg);
static void restack(dwm_screen_t* m);
static void run(void);
static void scan(void);
static int
sendevent(Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4);
static void sendmon(dwm_client_t* c, dwm_screen_t* m);
static void setclientstate(dwm_client_t* c, long state);
static void setfocus(dwm_client_t* c);
static void setfullscreen(dwm_client_t* c, int fullscreen);
static void setlayout(const Arg* arg);
static void setmfact(const Arg* arg);
static void setup(void);
static void seturgent(dwm_client_t* c, int urg);
static void showhide(dwm_client_t* c);
static void sigchld(int unused);
static void spawn(const Arg* arg);
static void tag(const Arg* arg);
static void tagmon(const Arg* arg);
static void tile(dwm_screen_t*);
static void togglebar(const Arg* arg);
static void togglefloating(const Arg* arg);
static void toggletag(const Arg* arg);
static void toggleview(const Arg* arg);
static void unfocus(dwm_client_t* c, int setfocus);
static void unmanage(dwm_client_t* c, int destroyed);
static void unmapnotify(XEvent* e);
static void updatebarpos(dwm_screen_t* m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(dwm_client_t* c);
static void updatetitle(dwm_client_t* c);
static void updatewindowtype(dwm_client_t* c);
static void updatewmhints(dwm_client_t* c);
static void view(const Arg* arg);
static dwm_client_t* wintoclient(Window w);
static dwm_screen_t* wintomon(Window w);
static int xerror(Display* dwm_x_display, XErrorEvent* ee);
static int xerrordummy(Display* dwm_x_display, XErrorEvent* ee);
static int xerrorstart(Display* dwm_x_display, XErrorEvent* ee);
static void zoom(const Arg* arg);
static void movestack(const Arg* arg);
static void togglescratch(const Arg* arg);
static unsigned int getsystraywidth();
static dwm_screen_t* systraytomon(dwm_screen_t* m);
static void updatesystrayicongeom(dwm_client_t* i, int w, int h);
static void updatesystrayiconstate(dwm_client_t* i, XPropertyEvent* ev);
static void updatesystray(void);
static dwm_client_t* wintosystrayicon(Window w);
static void removesystrayicon(dwm_client_t* i);
