# dwm - dynamic window manager
# See LICENSE file for copyright and license details.

# dwm version
VERSION = 6.1

# Customize below to fit your system

# paths
PREFIX = /usr
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

XINERAMALIBS  = -lXinerama
XINERAMAFLAGS = -DXINERAMA

FREETYPELIBS = -lfontconfig -lXft
FREETYPEINC = /usr/include/freetype2

INCS = -I${X11INC} -I${FREETYPEINC}
LIBS = -L${X11LIB} -lX11 ${XINERAMALIBS} ${FREETYPELIBS}

CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS}
CFLAGS   = -std=c11 -pedantic -Wall -Wno-deprecated-declarations -O3 -flto ${INCS} ${CPPFLAGS}
LDFLAGS  = -flto ${LIBS}

# compiler and linker
CC = clang-7

SRC = source/drw.c source/util.c
OBJ = ${SRC:.c=.o}

DWM_SRC = source/dwm_global.c
DWM_OBJ = ${DWM_SRC:.c=.o}

all: options dwm dmenu stest

options:
	@echo dwm build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

%.o: %.c
	${CC} -c ${CFLAGS} -o $@ $<

drw.o: source/drw.h
util.o: source/util.h
dwm.o: config.h source/core.h source/dwm_global.h
dmenu.o: dmenu_config.h

dwm: source/dwm.o ${OBJ} ${DWM_OBJ}
	${CC} -o $@ $^ ${LDFLAGS}

dmenu: source/dmenu.o ${OBJ}
	${CC} -o $@ $^ ${LDFLAGS}

stest: source/stest.o
	${CC} -o $@ $^ ${LDFLAGS}

clean:
	rm -f dwm source/dwm.o dmenu source/dmenu.o stest source/stest.o ${OBJ} ${DWM_OBJ} dwm-${VERSION}.tar.gz

dist: clean
	mkdir -p dwm-${VERSION}
	cp -R LICENSE Makefile README config.def.h \
		dwm.1 drw.h util.h ${SRC} dwm.png transient.c dwm-${VERSION}
	tar -cf dwm-${VERSION}.tar dwm-${VERSION}
	gzip dwm-${VERSION}.tar
	rm -rf dwm-${VERSION}

install: all
	mkdir -p ${PREFIX}/bin
	cp -f dwm ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/dwm
	cp -f dmenu ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/dmenu
	cp -f dmenu_run ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/dmenu_run
	cp -f stest ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/stest
	cp -f dwm.desktop ${PREFIX}/share/xsessions

uninstall:
	rm -f ${PREFIX}/bin/dwm
	rm -f ${PREFIX}/bin/dmenu
	rm -f ${PREFIX}/bin/dmenu_run
	rm -f ${PREFIX}/bin/stest
	rm -f ${PREFIX}/share/xsessions/dwm.desktop

.PHONY: all options clean dist install uninstall
