Yuki's dwm
==========

This is a fork of [suckless dwm](https://dwm.suckless.org/).

![Screen](screenshots/screen.jpg?raw=true "Screen")

Build
-----

```
make
make install
```

Features
--------

* Auto startup script
* [Dmenu](https://tools.suckless.org/dmenu/)
* Scratchpad
* Systray


# Rendering emoji crash

```
Jan 15 14:58:20 box /usr/lib/gdm3/gdm-x-session[65862]: dwm: fatal error: request code=140, error code=16; BadLength (poly request too large or internal Xlib length error)
Jan 15 14:58:20 box /usr/lib/gdm3/gdm-x-session[65862]: Obtained 10 stack frames.
Jan 15 14:58:20 box /usr/lib/gdm3/gdm-x-session[65862]:     dwm(+0xb95d) [0x55a8a860b95d]
Jan 15 14:58:20 box /usr/lib/gdm3/gdm-x-session[65862]:     dwm(+0xbbb8) [0x55a8a860bbb8]
Jan 15 14:58:20 box /usr/lib/gdm3/gdm-x-session[65862]:     /lib/x86_64-linux-gnu/libX11.so.6(_XError+0xfb) [0x7f8a9b93b0db]
Jan 15 14:58:20 box /usr/lib/gdm3/gdm-x-session[65862]:     /lib/x86_64-linux-gnu/libX11.so.6(+0x3de47) [0x7f8a9b937e47]
Jan 15 14:58:20 box /usr/lib/gdm3/gdm-x-session[65862]:     /lib/x86_64-linux-gnu/libX11.so.6(+0x3dee5) [0x7f8a9b937ee5]
Jan 15 14:58:20 box /usr/lib/gdm3/gdm-x-session[65862]:     /lib/x86_64-linux-gnu/libX11.so.6(_XReply+0x23d) [0x7f8a9b938e6d]
Jan 15 14:58:20 box /usr/lib/gdm3/gdm-x-session[65862]:     /lib/x86_64-linux-gnu/libX11.so.6(XSync+0x51) [0x7f8a9b934641]
Jan 15 14:58:20 box /usr/lib/gdm3/gdm-x-session[65862]:     dwm(+0xe202) [0x55a8a860e202]
Jan 15 14:58:20 box /usr/lib/gdm3/gdm-x-session[65862]:     dwm(+0x7690) [0x55a8a8607690]
Jan 15 14:58:20 box /usr/lib/gdm3/gdm-x-session[65862]:     dwm(+0x9279) [0x55a8a8609279]
Jan 15 14:58:20 box /usr/lib/gdm3/gdm-x-session[65862]: X Error of failed request:  BadLength (poly request too large or internal Xlib length error)
Jan 15 14:58:20 box /usr/lib/gdm3/gdm-x-session[65862]:   Major opcode of failed request:  140 (RENDER)
Jan 15 14:58:20 box /usr/lib/gdm3/gdm-x-session[65862]:   Minor opcode of failed request:  20 (RenderAddGlyphs)
Jan 15 14:58:20 box /usr/lib/gdm3/gdm-x-session[65862]:   Serial number of failed request:  3669
Jan 15 14:58:20 box /usr/lib/gdm3/gdm-x-session[65862]:   Current serial number in output stream:  3676
Jan 15 14:58:20 box nm-applet[65926]: nm-applet: Fatal IO error 11 (Resource temporarily unavailable) on X server :0.
Jan 15 14:58:20 box /usr/lib/gdm3/gdm-x-session[65932]: XIO:  fatal IO error 2 (No such file or directory) on X server ":0"
Jan 15 14:58:20 box /usr/lib/gdm3/gdm-x-session[65932]:       after 30 requests (29 known processed) with 0 events remaining.
```

```
> addr2line -e ../dwm 0xe1d1 0x7690 0x9279
/home/yuki/yuki/dwm/source/drw.c:371
/home/yuki/yuki/dwm/source/dwm.c:756
/home/yuki/yuki/dwm/source/dwm.c:1209
```

## Solution

Decribed at https://unix.stackexchange.com/questions/629281/gitk-crashes-when-viewing-commit-containing-emoji-x-error-of-failed-request-ba

```
apt remove --purge fonts-noto-color-emoji
```
