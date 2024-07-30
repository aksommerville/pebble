#ifndef XEGL_INTERNAL_H
#define XEGL_INTERNAL_H

#include "pblrt/pblrt_drivers.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>

#if USE_xinerama
  #include <X11/extensions/Xinerama.h>
#endif

#define KeyRepeat (LASTEvent+2)
#define XEGL_KEY_REPEAT_INTERVAL 10
#define XEGL_ICON_SIZE_LIMIT 64

struct pblrt_video_xegl {
  struct pblrt_video hdr;
  
  Display *dpy;
  int screen;
  Window win;
  
  EGLDisplay egldisplay;
  EGLContext eglcontext;
  EGLSurface eglsurface;
  int version_major,version_minor;
  
  Atom atom_WM_PROTOCOLS;
  Atom atom_WM_DELETE_WINDOW;
  Atom atom__NET_WM_STATE;
  Atom atom__NET_WM_STATE_FULLSCREEN;
  Atom atom__NET_WM_STATE_ADD;
  Atom atom__NET_WM_STATE_REMOVE;
  Atom atom__NET_WM_ICON;
  Atom atom__NET_WM_ICON_NAME;
  Atom atom__NET_WM_NAME;
  Atom atom_WM_CLASS;
  Atom atom_STRING;
  Atom atom_UTF8_STRING;
  
  int screensaver_suppressed;
  int focus;
  
  GLuint texid;
};

#define DRIVER ((struct pblrt_video_xegl*)driver)

int xegl_update(struct pblrt_video *driver);
int xegl_usb_usage_from_keysym(int keysym);

#endif
