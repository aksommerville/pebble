#ifndef BCM_INTERNAL_H
#define BCM_INTERNAL_H

#include <bcm_host.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <unistd.h>
#include "pblrt/pblrt_drivers.h"

#define BCM_FRAMEBUFFER_SIZE_LIMIT 4096

struct pblrt_video_bcm {
  struct pblrt_video hdr;
  DISPMANX_DISPLAY_HANDLE_T vcdisplay;
  DISPMANX_ELEMENT_HANDLE_T vcelement;
  DISPMANX_UPDATE_HANDLE_T vcupdate;
  EGL_DISPMANX_WINDOW_T eglwindow;
  EGLDisplay egldisplay;
  EGLSurface eglsurface;
  EGLContext eglcontext;
  EGLConfig eglconfig;
  GLint program;
  GLuint texid;
  int texfilter;
  GLint u_screensize;
  GLint u_sampler;
  int dstx,dsty,dstw,dsth;
  int dstww,dstwh,dstfw,dstfh;
};

#define DRIVER ((struct pblrt_video_bcm*)driver)

void bcm_render_cleanup(struct pblrt_video *driver);
int bcm_render_init(struct pblrt_video *driver);
int bcm_render_commit_framebuffer(struct pblrt_video *driver,const void *fb,int fbw,int fbh);

#endif
