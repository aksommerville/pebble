#ifndef BCM_INTERNAL_H
#define BCM_INTERNAL_H

#include <bcm_host.h>
//#include <EGL/egl.h>
#include <stdio.h>
#include <unistd.h>
#include "pblrt/pblrt_drivers.h"

struct pblrt_video_bcm {
  struct pblrt_video hdr;
  DISPMANX_DISPLAY_HANDLE_T vcdisplay;
  DISPMANX_RESOURCE_HANDLE_T vcresource;// For framebuffer only.
  DISPMANX_ELEMENT_HANDLE_T vcelement;
  DISPMANX_UPDATE_HANDLE_T vcupdate;
  volatile int vsync_seq;
  void *fb;
  int fbw,fbh;
  #if 0
  EGL_DISPMANX_WINDOW_T eglwindow;
  EGLDisplay egldisplay;
  EGLSurface eglsurface;
  EGLContext eglcontext;
  EGLConfig eglconfig;
  #endif
};

#define DRIVER ((struct pblrt_video_bcm*)driver)

#endif
