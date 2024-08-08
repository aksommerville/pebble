#ifndef DRMGX_INTERNAL_H
#define DRMGX_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include "pblrt/pblrt_drivers.h"

#define DRMGX_FRAMEBUFFER_SIZE_LIMIT 4096

struct drmgx_fb {
  uint32_t fbid;
  int handle;
  int size;
};

struct pblrt_video_drmgx {
  struct pblrt_video hdr;
  int fd;
  
  int mmw,mmh; // monitor's physical size
  int rate; // monitor's refresh rate in hertz
  drmModeModeInfo mode; // ...and more in that line
  int connid,encid,crtcid;
  drmModeCrtcPtr crtc_restore;
  
  int flip_pending;
  struct drmgx_fb fbv[2];
  int fbp;
  struct gbm_bo *bo_pending;
  int crtcunset;
  
  struct gbm_device *gbmdevice;
  struct gbm_surface *gbmsurface;
  EGLDisplay egldisplay;
  EGLContext eglcontext;
  EGLSurface eglsurface;
  
  GLuint texid;
  int texfilter;
  GLint program;
  GLuint u_screensize,u_sampler;
  int dstx,dsty,dstw,dsth;
  int dstww,dstwh,dstfw,dstfh;
};

#define DRIVER ((struct pblrt_video_drmgx*)driver)

int drmgx_open_file(struct pblrt_video *driver,const char *path);
int drmgx_configure(struct pblrt_video *driver);
int drmgx_init_gx(struct pblrt_video *driver);

void drmgx_render_cleanup(struct pblrt_video *driver);
int drmgx_render_init(struct pblrt_video *driver);
int drmgx_render_commit(struct pblrt_video *driver,const void *fb,int fbw,int fbh);

#endif
