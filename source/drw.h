/* See LICENSE file for copyright and license details. */

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>

typedef struct { Cursor cursor; } Cur;

typedef struct Fnt {
  Display* dpy;
  unsigned int h;
  XftFont* xfont;
  FcPattern* pattern;
  struct Fnt* next;
} Fnt;

typedef struct dwm_drw_s {
  unsigned int w, h;
  Display* dpy;
  int screen;
  Window root;
  Drawable drawable;
  GC gc;
  XftColor* scheme;
  Fnt* fonts;
} dwm_drw_t;

/* Drawable abstraction */
void dwm_init_dwm_drw(dwm_drw_t* dwm_drw,
                      Display* dpy,
                      int screen,
                      Window root,
                      unsigned int w,
                      unsigned int h);
void dwm_release_dwm_drw(dwm_drw_t* dwm_drw);
void dwm_drw_resize(dwm_drw_t* dwm_drw, unsigned int w, unsigned int h);

/* Fnt abstraction */
Fnt* dwm_drw_fontset_create(dwm_drw_t* dwm_drw, const char* fonts[], size_t fontcount);
void dwm_drw_fontset_free(Fnt* set);
unsigned int dwm_drw_fontset_getwidth(dwm_drw_t* dwm_drw, const char* text);
void dwm_drw_font_getexts(
  Fnt* font, const char* text, unsigned int len, unsigned int* w, unsigned int* h);

/* Colorscheme abstraction */
void dwm_drw_clr_create(dwm_drw_t* dwm_drw, XftColor* dest, const char* clrname);
XftColor* dwm_drw_scm_create(dwm_drw_t* dwm_drw, const char* clrnames[], size_t clrcount);

/* Cursor abstraction */
Cur* dwm_drw_cur_create(dwm_drw_t* dwm_drw, int shape);
void dwm_drw_cur_free(dwm_drw_t* dwm_drw, Cur* cursor);

/* Drawing context manipulation */
void dwm_drw_setfontset(dwm_drw_t* dwm_drw, Fnt* set);
void dwm_drw_setscheme(dwm_drw_t* dwm_drw, XftColor* scm);

/* Drawing functions */
void dwm_drw_rect(dwm_drw_t* dwm_drw,
                  int x,
                  int y,
                  unsigned int w,
                  unsigned int h,
                  int filled,
                  int invert);
int dwm_drw_text(dwm_drw_t* dwm_drw,
                 int x,
                 int y,
                 unsigned int w,
                 unsigned int h,
                 unsigned int lpad,
                 const char* text,
                 int invert);

/* Map functions */
void dwm_drw_map(
  dwm_drw_t* dwm_drw, Window win, int x, int y, unsigned int w, unsigned int h);
