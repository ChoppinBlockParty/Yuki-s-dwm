/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drw.h"
#include "util.h"

#define UTF_INVALID 0xFFFD
#define UTF_SIZ 4

enum { DwmFgColor, DwmBgColor };

static const unsigned char utfbyte[UTF_SIZ + 1] = {0x80, 0, 0xC0, 0xE0, 0xF0};
static const unsigned char utfmask[UTF_SIZ + 1] = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
static const long utfmin[UTF_SIZ + 1] = {0, 0, 0x80, 0x800, 0x10000};
static const long utfmax[UTF_SIZ + 1] = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};

static long utf8decodebyte(const char c, size_t* i) {
  for (*i = 0; *i < (UTF_SIZ + 1); ++(*i))
    if (((unsigned char)c & utfmask[*i]) == utfbyte[*i])
      return (unsigned char)c & ~utfmask[*i];
  return 0;
}

static size_t utf8validate(long* u, size_t i) {
  if (!BETWEEN(*u, utfmin[i], utfmax[i]) || BETWEEN(*u, 0xD800, 0xDFFF))
    *u = UTF_INVALID;
  for (i = 1; *u > utfmax[i]; ++i)
    ;
  return i;
}

static size_t utf8decode(const char* c, long* u, size_t clen) {
  size_t i, j, len, type;
  long udecoded;

  *u = UTF_INVALID;
  if (!clen)
    return 0;
  udecoded = utf8decodebyte(c[0], &len);
  if (!BETWEEN(len, 1, UTF_SIZ))
    return 1;
  for (i = 1, j = 1; i < clen && j < len; ++i, ++j) {
    udecoded = (udecoded << 6) | utf8decodebyte(c[i], &type);
    if (type)
      return j;
  }
  if (j < len)
    return 0;
  *u = udecoded;
  utf8validate(u, len);

  return len;
}

void dwm_init_dwm_drw(dwm_drw_t* dwm_drw,
                      Display* dpy,
                      int screen,
                      Window root,
                      unsigned int w,
                      unsigned int h) {
  dwm_drw->dpy = dpy;
  dwm_drw->screen = screen;
  dwm_drw->root = root;
  dwm_drw->w = w;
  dwm_drw->h = h;
  dwm_drw->drawable = XCreatePixmap(dpy, root, w, h, DefaultDepth(dpy, screen));
  dwm_drw->gc = XCreateGC(dpy, root, 0, NULL);
  XSetLineAttributes(dpy, dwm_drw->gc, 1, LineSolid, CapButt, JoinMiter);
}

void dwm_release_dwm_drw(dwm_drw_t* dwm_drw) {
  XFreePixmap(dwm_drw->dpy, dwm_drw->drawable);
  XFreeGC(dwm_drw->dpy, dwm_drw->gc);
}

void dwm_drw_resize(dwm_drw_t* dwm_drw, unsigned int w, unsigned int h) {
  if (!dwm_drw)
    return;

  dwm_drw->w = w;
  dwm_drw->h = h;
  if (dwm_drw->drawable)
    XFreePixmap(dwm_drw->dpy, dwm_drw->drawable);
  dwm_drw->drawable = XCreatePixmap(
    dwm_drw->dpy, dwm_drw->root, w, h, DefaultDepth(dwm_drw->dpy, dwm_drw->screen));
}

/* This function is an implementation detail. Library users should use
 * drw_fontset_create instead.
 */
static Fnt*
xfont_create(dwm_drw_t* dwm_drw, const char* fontname, FcPattern* fontpattern) {
  Fnt* font;
  XftFont* xfont = NULL;
  FcPattern* pattern = NULL;

  if (fontname) {
    /* Using the pattern found at font->xfont->pattern does not yield the
		 * same substitution results as using the pattern returned by
		 * FcNameParse; using the latter results in the desired fallback
		 * behaviour whereas the former just results in missing-character
		 * rectangles being drawn, at least with some fonts. */
    if (!(xfont = XftFontOpenName(dwm_drw->dpy, dwm_drw->screen, fontname))) {
      fprintf(stderr, "error, cannot load font from name: '%s'\n", fontname);
      return NULL;
    }
    if (!(pattern = FcNameParse((FcChar8*)fontname))) {
      fprintf(stderr, "error, cannot parse font name to pattern: '%s'\n", fontname);
      XftFontClose(dwm_drw->dpy, xfont);
      return NULL;
    }
  } else if (fontpattern) {
    if (!(xfont = XftFontOpenPattern(dwm_drw->dpy, fontpattern))) {
      fprintf(stderr, "error, cannot load font from pattern.\n");
      return NULL;
    }
  } else {
    die("no font specified.");
  }

  font = ecalloc(1, sizeof(Fnt));
  font->xfont = xfont;
  font->pattern = pattern;
  font->h = xfont->ascent + xfont->descent;
  font->dpy = dwm_drw->dpy;

  return font;
}

static void xfont_free(Fnt* font) {
  if (!font)
    return;
  if (font->pattern)
    FcPatternDestroy(font->pattern);
  XftFontClose(font->dpy, font->xfont);
  free(font);
}

Fnt* dwm_drw_fontset_create(dwm_drw_t* dwm_drw, const char* fonts[], size_t fontcount) {
  Fnt *cur, *ret = NULL;
  size_t i;

  if (!dwm_drw || !fonts)
    return NULL;

  for (i = 1; i <= fontcount; i++) {
    if ((cur = xfont_create(dwm_drw, fonts[fontcount - i], NULL))) {
      cur->next = ret;
      ret = cur;
    }
  }
  return (dwm_drw->fonts = ret);
}

void dwm_drw_fontset_free(Fnt* font) {
  if (font) {
    dwm_drw_fontset_free(font->next);
    xfont_free(font);
  }
}

void dwm_drw_clr_create(dwm_drw_t* dwm_drw, XftColor* dest, const char* clrname) {
  if (!dwm_drw || !dest || !clrname)
    return;

  if (!XftColorAllocName(dwm_drw->dpy,
                         DefaultVisual(dwm_drw->dpy, dwm_drw->screen),
                         DefaultColormap(dwm_drw->dpy, dwm_drw->screen),
                         clrname,
                         dest))
    die("error, cannot allocate color '%s'", clrname);
}

/* Wrapper to create color schemes. The caller has to call free(3) on the
 * returned color scheme when done using it. */
XftColor*
dwm_drw_scm_create(dwm_drw_t* dwm_drw, const char* clrnames[], size_t clrcount) {
  size_t i;
  XftColor* ret;

  /* need at least two colors for a scheme */
  if (!dwm_drw || !clrnames || clrcount < 2
      || !(ret = ecalloc(clrcount, sizeof(XftColor))))
    return NULL;

  for (i = 0; i < clrcount; i++)
    dwm_drw_clr_create(dwm_drw, &ret[i], clrnames[i]);
  return ret;
}

void dwm_drw_setfontset(dwm_drw_t* dwm_drw, Fnt* set) {
  if (dwm_drw)
    dwm_drw->fonts = set;
}

void dwm_drw_setscheme(dwm_drw_t* dwm_drw, XftColor* scm) {
  if (dwm_drw)
    dwm_drw->scheme = scm;
}

void dwm_drw_rect(dwm_drw_t* dwm_drw,
                  int x,
                  int y,
                  unsigned int w,
                  unsigned int h,
                  int filled,
                  int invert) {
  if (!dwm_drw || !dwm_drw->scheme)
    return;
  XSetForeground(dwm_drw->dpy,
                 dwm_drw->gc,
                 invert ? dwm_drw->scheme[DwmBgColor].pixel
                        : dwm_drw->scheme[DwmFgColor].pixel);
  if (filled)
    XFillRectangle(dwm_drw->dpy, dwm_drw->drawable, dwm_drw->gc, x, y, w, h);
  else
    XDrawRectangle(dwm_drw->dpy, dwm_drw->drawable, dwm_drw->gc, x, y, w - 1, h - 1);
}

int dwm_drw_text(dwm_drw_t* dwm_drw,
                 int x,
                 int y,
                 unsigned int w,
                 unsigned int h,
                 unsigned int lpad,
                 const char* text,
                 int invert) {
  char buf[1024];
  int ty;
  unsigned int ew;
  XftDraw* d = NULL;
  Fnt *usedfont, *curfont, *nextfont;
  size_t i, len;
  int utf8strlen, utf8charlen, render = x || y || w || h;
  long utf8codepoint = 0;
  const char* utf8str;
  FcCharSet* fccharset;
  FcPattern* fcpattern;
  FcPattern* match;
  XftResult result;
  int charexists = 0;

  if (!dwm_drw || (render && !dwm_drw->scheme) || !text || !dwm_drw->fonts)
    return 0;

  if (!render) {
    w = ~w;
  } else {
    XSetForeground(
      dwm_drw->dpy, dwm_drw->gc, dwm_drw->scheme[invert ? DwmFgColor : DwmBgColor].pixel);
    XFillRectangle(dwm_drw->dpy, dwm_drw->drawable, dwm_drw->gc, x, y, w, h);
    d = XftDrawCreate(dwm_drw->dpy,
                      dwm_drw->drawable,
                      DefaultVisual(dwm_drw->dpy, dwm_drw->screen),
                      DefaultColormap(dwm_drw->dpy, dwm_drw->screen));
    x += lpad;
    w -= lpad;
  }

  usedfont = dwm_drw->fonts;
  while (1) {
    utf8strlen = 0;
    utf8str = text;
    nextfont = NULL;
    while (*text) {
      utf8charlen = utf8decode(text, &utf8codepoint, UTF_SIZ);
      for (curfont = dwm_drw->fonts; curfont; curfont = curfont->next) {
        charexists
          = charexists || XftCharExists(dwm_drw->dpy, curfont->xfont, utf8codepoint);
        if (charexists) {
          if (curfont == usedfont) {
            utf8strlen += utf8charlen;
            text += utf8charlen;
          } else {
            nextfont = curfont;
          }
          break;
        }
      }

      if (!charexists || nextfont)
        break;
      else
        charexists = 0;
    }

    if (utf8strlen) {
      dwm_drw_font_getexts(usedfont, utf8str, utf8strlen, &ew, NULL);
      /* shorten text if necessary */
      for (len = MIN(utf8strlen, sizeof(buf) - 1); len && ew > w; len--)
        dwm_drw_font_getexts(usedfont, utf8str, len, &ew, NULL);

      if (len) {
        memcpy(buf, utf8str, len);
        buf[len] = '\0';
        if (len < utf8strlen)
          for (i = len; i && i > len - 3; buf[--i] = '.')
            ; /* NOP */

        if (render) {
          ty = y + (h - usedfont->h) / 2 + usedfont->xfont->ascent;
          XftDrawStringUtf8(d,
                            &dwm_drw->scheme[invert ? DwmBgColor : DwmFgColor],
                            usedfont->xfont,
                            x,
                            ty,
                            (XftChar8*)buf,
                            len);
        }
        x += ew;
        w -= ew;
      }
    }

    if (!*text) {
      break;
    } else if (nextfont) {
      charexists = 0;
      usedfont = nextfont;
    } else {
      /* Regardless of whether or not a fallback font is found, the
			 * character must be drawn. */
      charexists = 1;

      fccharset = FcCharSetCreate();
      FcCharSetAddChar(fccharset, utf8codepoint);

      if (!dwm_drw->fonts->pattern) {
        /* Refer to the comment in xfont_create for more information. */
        die("the first font in the cache must be loaded from a font string.");
      }

      fcpattern = FcPatternDuplicate(dwm_drw->fonts->pattern);
      FcPatternAddCharSet(fcpattern, FC_CHARSET, fccharset);
      FcPatternAddBool(fcpattern, FC_SCALABLE, FcTrue);

      FcConfigSubstitute(NULL, fcpattern, FcMatchPattern);
      FcDefaultSubstitute(fcpattern);
      match = XftFontMatch(dwm_drw->dpy, dwm_drw->screen, fcpattern, &result);

      FcCharSetDestroy(fccharset);
      FcPatternDestroy(fcpattern);

      if (match) {
        usedfont = xfont_create(dwm_drw, NULL, match);
        if (usedfont && XftCharExists(dwm_drw->dpy, usedfont->xfont, utf8codepoint)) {
          for (curfont = dwm_drw->fonts; curfont->next; curfont = curfont->next)
            ; /* NOP */
          curfont->next = usedfont;
        } else {
          xfont_free(usedfont);
          usedfont = dwm_drw->fonts;
        }
      }
    }
  }
  if (d)
    XftDrawDestroy(d);

  return x + (render ? w : 0);
}

void dwm_drw_map(
  dwm_drw_t* dwm_drw, Window win, int x, int y, unsigned int w, unsigned int h) {
  if (!dwm_drw)
    return;

  XCopyArea(dwm_drw->dpy, dwm_drw->drawable, win, dwm_drw->gc, x, y, w, h, x, y);
  XSync(dwm_drw->dpy, False);
}

unsigned int dwm_drw_fontset_getwidth(dwm_drw_t* dwm_drw, const char* text) {
  if (!dwm_drw || !dwm_drw->fonts || !text)
    return 0;
  return dwm_drw_text(dwm_drw, 0, 0, 0, 0, 0, text, 0);
}

void dwm_drw_font_getexts(
  Fnt* font, const char* text, unsigned int len, unsigned int* w, unsigned int* h) {
  XGlyphInfo ext;

  if (!font || !text)
    return;

  XftTextExtentsUtf8(font->dpy, font->xfont, (XftChar8*)text, len, &ext);
  if (w)
    *w = ext.xOff;
  if (h)
    *h = font->h;
}

Cur* dwm_drw_cur_create(dwm_drw_t* dwm_drw, int shape) {
  Cur* cur;

  if (!dwm_drw || !(cur = ecalloc(1, sizeof(Cur))))
    return NULL;

  cur->cursor = XCreateFontCursor(dwm_drw->dpy, shape);

  return cur;
}

void dwm_drw_cur_free(dwm_drw_t* dwm_drw, Cur* cursor) {
  if (!cursor)
    return;

  XFreeCursor(dwm_drw->dpy, cursor->cursor);
  free(cursor);
}
