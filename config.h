/* See LICENSE file for copyright and license details. */

#include <stddef.h>

#include "core.h"

/* appearance */
static const unsigned int borderpx = 1; /* border pixel of windows */
static const unsigned int snap = 32; /* snap pixel */
static const int showbar = 1; /* 0 means no bar */
static const int topbar = 1; /* 0 means bottom bar */
static const unsigned int systraypinning
  = 0; /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static const unsigned int systrayspacing = 2; /* systray spacing */
static const int systraypinningfailfirst
  = 1; /* 1: if pinning fails, display systray on the first monitor, 0: display systray on the last monitor*/
static const int showsystray = 1; /* 0 means no systray */
static const char* fonts[] = {"monospace:size=10"};
static const char dmenufont[] = "monospace:size=10";
static const char col_gray1[] = "#222222";
static const char col_gray2[] = "#444444";
static const char col_gray3[] = "#bbbbbb";
static const char col_gray4[] = "#eeeeee";
static const char col_cyan[] = "#005577";
static const char* colors[][3] = {
    /*               fg         bg         border   */
    [SchemeNorm] = {col_gray3, col_gray1, col_gray2},
    [SchemeSel] = {col_gray4, col_cyan, col_cyan},
};

/* tagging */
static const char* tags[] = {"ᛝ", "ᛤ", "ᛄ", "ᛪ", "ᚸ", "ᛔ", "ᚌ", "ᛃ", "ᛗ"};

// clang-format off
static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor */
	{ "Gimp",     NULL,       NULL,       0,            1,           -1 },
	/* { "Firefox",  NULL,       NULL,       1 << 8,       0,           -1 }, */
};
// clang-format on

/* layout(s) */
static const float mfact = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster = 1; /* number of clients in master area */
static const int resizehints = 1; /* 1 means respect size hints in tiled resizals */

// clang-format off
static const Layout layouts[] = {
/*  symbol    arrange function */
	{ "#",      tile },    /* first entry is default */
	{ "F",      NULL },    /* no layout function means floating behavior */
	{ "@",      monocle },
};
// clang-format on

/* key definitions */
#define MODKEY Mod4Mask
// clang-format off
#define TAGKEYS(KEY, TAG)                                                \
  {MODKEY,                           KEY, view,       {.ui = 1 << TAG}}, \
  {MODKEY | ControlMask,             KEY, toggleview, {.ui = 1 << TAG}}, \
  {MODKEY | ShiftMask,               KEY, tag,        {.ui = 1 << TAG}}, \
  {MODKEY | ControlMask | ShiftMask, KEY, toggletag,  {.ui = 1 << TAG}},
// clang-format on

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd)                                                                       \
  {                                                                                      \
    .v = (const char* []) { "/bin/sh", "-c", cmd, NULL }                                 \
  }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char* dmenucmd[] = {"dmenu_run",
                                 "-m",
                                 dmenumon,
                                 "-fn",
                                 dmenufont,
                                 "-nb",
                                 col_gray1,
                                 "-nf",
                                 col_gray3,
                                 "-sb",
                                 col_cyan,
                                 "-sf",
                                 col_gray4,
                                 NULL};
// Start Emacs from zsh because environment are important for it
static const char* emacscmd[] = {"/bin/zsh", "-c", "emacs", NULL};
static const char* firefoxcmd[] = {"firefox", NULL};
static const char* termcmd[] = {"urxvt", NULL};

// clang-format off
static Key keys[] = {
	/* modifier         key           function        argument */
	{ 0,                XK_F1,           togglescratch,  {.v = termcmd } },
	{ MODKEY,           XK_space,        spawn,          {.v = dmenucmd } },
	{ MODKEY,           XK_bracketleft,  spawn,          {.v = emacscmd } },
	{ MODKEY,           XK_bracketright, spawn,          {.v = firefoxcmd } },
	{ MODKEY,           XK_Return,       spawn,          {.v = termcmd } },
	{ MODKEY,           XK_z,            togglebar,      {0} },
	{ MODKEY,           XK_n,            focusstack,     {.i = +1 } },
	{ MODKEY,           XK_p,            focusstack,     {.i = -1 } },
	{ MODKEY,           XK_l,            incnmaster,     {.i = +1 } },
	{ MODKEY,           XK_h,            incnmaster,     {.i = -1 } },
	{ MODKEY,           XK_minus,        setmfact,       {.f = -0.05} },
	{ MODKEY,           XK_equal,        setmfact,       {.f = +0.05} },
	{ MODKEY,           XK_u,            zoom,           {0} },
	{ MODKEY,           XK_o,            view,           {0} },
	{ MODKEY,           XK_grave,        killclient,     {0} },
	{ MODKEY,           XK_t,            setlayout,      {.v = &layouts[0]} },
	{ MODKEY,           XK_b,            setlayout,      {.v = &layouts[1]} },
	{ MODKEY,           XK_f,            setlayout,      {.v = &layouts[2]} },
	{ MODKEY,           XK_c,            togglefloating, {0} },
	{ MODKEY,           XK_0,            view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask, XK_0,            tag,            {.ui = ~0 } },
	{ MODKEY,           XK_k,            focusmon,       {.i = -1 } },
	{ MODKEY,           XK_j,            focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask, XK_k,            tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask, XK_j,            tagmon,         {.i = +1 } },
	{ MODKEY,           XK_m,            movestack,      {.i = -1 } },
	{ MODKEY,           XK_comma,        movestack,      {.i = +1 } },
	TAGKEYS(            XK_1,                            0)
	TAGKEYS(            XK_2,                            1)
	TAGKEYS(            XK_3,                            2)
	TAGKEYS(            XK_q,                            3)
	TAGKEYS(            XK_w,                            4)
	TAGKEYS(            XK_e,                            5)
	TAGKEYS(            XK_a,                            6)
	TAGKEYS(            XK_s,                            7)
	TAGKEYS(            XK_d,                            8)

	{ MODKEY|ControlMask|ShiftMask, XK_r, quit, {0} },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]} },
	{ ClkWinTitle,          0,              Button2,        zoom,           {0} },
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = termcmd } },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
};
// clang-format on
