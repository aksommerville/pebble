#include "xegl_internal.h"

/* Delete.
 */

static void xegl_del(struct pblrt_video *driver) {
  xegl_render_cleanup(driver);
  eglDestroyContext(DRIVER->egldisplay,DRIVER->eglcontext);
  XCloseDisplay(DRIVER->dpy);
}

/* Estimate monitor size.
 * With Xinerama in play, we look for the smallest monitor.
 * Otherwise we end up with the logical desktop size, which is really never what you want.
 * Never fails to return something positive, even if it's a wild guess.
 */
 
static void xegl_estimate_monitor_size(int *w,int *h,const struct pblrt_video *driver) {
  *w=*h=0;
  #if USE_xinerama
    int infoc=0;
    XineramaScreenInfo *infov=XineramaQueryScreens(DRIVER->dpy,&infoc);
    if (infov) {
      if (infoc>0) {
        *w=infov[0].width;
        *h=infov[0].height;
        int i=infoc; while (i-->1) {
          if ((infov[i].width<*w)||(infov[i].height<*h)) {
            *w=infov[i].width;
            *h=infov[i].height;
          }
        }
      }
      XFree(infov);
    }
  #endif
  if ((*w<1)||(*h<1)) {
    *w=DisplayWidth(DRIVER->dpy,0);
    *h=DisplayHeight(DRIVER->dpy,0);
    if ((*w<1)||(*h<1)) {
      *w=640;
      *h=480;
    }
  }
}

/* Set window title.
 */
 
static void xegl_set_title(struct pblrt_video *driver,const char *src) {
  
  // I've seen these properties in GNOME 2, unclear whether they might still be in play:
  XTextProperty prop={.value=(void*)src,.encoding=DRIVER->atom_STRING,.format=8,.nitems=0};
  while (prop.value[prop.nitems]) prop.nitems++;
  XSetWMName(DRIVER->dpy,DRIVER->win,&prop);
  XSetWMIconName(DRIVER->dpy,DRIVER->win,&prop);
  XSetTextProperty(DRIVER->dpy,DRIVER->win,&prop,DRIVER->atom__NET_WM_ICON_NAME);
    
  // This one becomes the window title and bottom-bar label, in GNOME 3:
  prop.encoding=DRIVER->atom_UTF8_STRING;
  XSetTextProperty(DRIVER->dpy,DRIVER->win,&prop,DRIVER->atom__NET_WM_NAME);
    
  // This daffy bullshit becomes the Alt-Tab text in GNOME 3:
  {
    char tmp[256];
    int len=prop.nitems+1+prop.nitems;
    if (len<sizeof(tmp)) {
      memcpy(tmp,prop.value,prop.nitems);
      tmp[prop.nitems]=0;
      memcpy(tmp+prop.nitems+1,prop.value,prop.nitems);
      tmp[prop.nitems+1+prop.nitems]=0;
      prop.value=tmp;
      prop.nitems=prop.nitems+1+prop.nitems;
      prop.encoding=DRIVER->atom_STRING;
      XSetTextProperty(DRIVER->dpy,DRIVER->win,&prop,DRIVER->atom_WM_CLASS);
    }
  }
}

/* Set window icon.
 */
 
static void xegl_copy_icon_pixels(long *dst,const uint8_t *src,int c) {
  for (;c-->0;dst++,src+=4) {
    *dst=(src[3]<<24)|(src[0]<<16)|(src[1]<<8)|src[2];
  }
}
 
static void xegl_set_icon(struct pblrt_video *driver,const void *rgba,int w,int h) {
  if (!rgba||(w<1)||(h<1)||(w>XEGL_ICON_SIZE_LIMIT)||(h>XEGL_ICON_SIZE_LIMIT)) return;
  int length=2+w*h;
  long *pixels=malloc(sizeof(long)*length);
  if (!pixels) return;
  pixels[0]=w;
  pixels[1]=h;
  xegl_copy_icon_pixels(pixels+2,rgba,w*h);
  XChangeProperty(DRIVER->dpy,DRIVER->win,DRIVER->atom__NET_WM_ICON,XA_CARDINAL,32,PropModeReplace,(unsigned char*)pixels,length);
  free(pixels);
}

/* Hide cursor.
 */

static void xegl_hide_cursor(struct pblrt_video *driver) {
  XColor color;
  Pixmap pixmap=XCreateBitmapFromData(DRIVER->dpy,DRIVER->win,"\0\0\0\0\0\0\0\0",1,1);
  Cursor cursor=XCreatePixmapCursor(DRIVER->dpy,pixmap,pixmap,&color,&color,0,0);
  XDefineCursor(DRIVER->dpy,DRIVER->win,cursor);
  XFreeCursor(DRIVER->dpy,cursor);
  XFreePixmap(DRIVER->dpy,pixmap);
}

/* Initialize.
 */
 
static int xegl_init(struct pblrt_video *driver,const struct pblrt_video_setup *setup) {

  if (!(DRIVER->dpy=XOpenDisplay(setup->device))) return -1;
  DRIVER->screen=DefaultScreen(DRIVER->dpy);
  
  #define GETATOM(tag) DRIVER->atom_##tag=XInternAtom(DRIVER->dpy,#tag,0);
  GETATOM(WM_PROTOCOLS)
  GETATOM(WM_DELETE_WINDOW)
  GETATOM(_NET_WM_STATE)
  GETATOM(_NET_WM_STATE_FULLSCREEN)
  GETATOM(_NET_WM_STATE_ADD)
  GETATOM(_NET_WM_STATE_REMOVE)
  GETATOM(_NET_WM_ICON)
  GETATOM(_NET_WM_ICON_NAME)
  GETATOM(_NET_WM_NAME)
  GETATOM(STRING)
  GETATOM(UTF8_STRING)
  GETATOM(WM_CLASS)
  #undef GETATOM
  
  // Choose window size.
  if ((setup->w>0)&&(setup->h>0)) {
    DRIVER->hdr.w=setup->w;
    DRIVER->hdr.h=setup->h;
  } else {
    int monw,monh;
    xegl_estimate_monitor_size(&monw,&monh,driver);
    // There's usually some fixed chrome on both the main screen and around the window.
    // If we got a reasonable size, cut it to 3/4 to accomodate that chrome.
    if ((monw>=256)&&(monh>=256)) {
      monw=(monw*3)>>2;
      monh=(monh*3)>>2;
    }
    if ((setup->fbw>0)&&(setup->fbh>0)) {
      int xscale=monw/setup->fbw;
      int yscale=monh/setup->fbh;
      int scale=(xscale<yscale)?xscale:yscale;
      if (scale<1) scale=1;
      DRIVER->hdr.w=setup->fbw*scale;
      DRIVER->hdr.h=setup->fbh*scale;
    } else {
      DRIVER->hdr.w=monw;
      DRIVER->hdr.h=monh;
    }
  }
  
  // Create EGL context.
  EGLAttrib egldpyattr[]={
    EGL_NONE,
  };
  if (!(DRIVER->egldisplay=eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR,DRIVER->dpy,egldpyattr))) return -1;
  if (!eglInitialize(DRIVER->egldisplay,&DRIVER->version_major,&DRIVER->version_minor)) return -1;
  EGLint eglattribv[]={
    EGL_BUFFER_SIZE,     32,
    EGL_RED_SIZE,         8,
    EGL_GREEN_SIZE,       8,
    EGL_BLUE_SIZE,        8,
    EGL_ALPHA_SIZE,       0,
    EGL_DEPTH_SIZE,      EGL_DONT_CARE,
    EGL_STENCIL_SIZE,    EGL_DONT_CARE,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_SURFACE_TYPE,    EGL_WINDOW_BIT|EGL_PIXMAP_BIT,
    EGL_NONE,
  };
  EGLConfig eglconfig=0;
  int eglcfgc=0;
  if (!eglChooseConfig(DRIVER->egldisplay,eglattribv,&eglconfig,1,&eglcfgc)) return -1;
  if (eglcfgc<1) return -1;
  if (!eglBindAPI(EGL_OPENGL_ES_API)) return -1;
  EGLint ctxattribv[]={
    EGL_CONTEXT_MAJOR_VERSION,2,
    EGL_CONTEXT_MINOR_VERSION,0,
    EGL_NONE,
  };
  if (!(DRIVER->eglcontext=eglCreateContext(DRIVER->egldisplay,eglconfig,0,ctxattribv))) return -1;
  
  // Create window.
  XSetWindowAttributes wattr={
    .event_mask=
      StructureNotifyMask|
      KeyPressMask|KeyReleaseMask|
      FocusChangeMask|
    0,
  };
  DRIVER->win=XCreateWindow(
    DRIVER->dpy,RootWindow(DRIVER->dpy,DRIVER->screen),
    0,0,driver->w,driver->h,0,
    CopyFromParent,InputOutput,CopyFromParent,
    CWBorderPixel|CWColormap|CWEventMask,&wattr
  );
  if (!DRIVER->win) return -1;
  
  // Create EGL surface and context.
  EGLAttrib eglwinattr[]={
    EGL_NONE,
  };
  if (!(DRIVER->eglsurface=eglCreatePlatformWindowSurface(DRIVER->egldisplay,eglconfig,&DRIVER->win,eglwinattr))) return -1;
  eglMakeCurrent(DRIVER->egldisplay,DRIVER->eglsurface,DRIVER->eglsurface,DRIVER->eglcontext);
  
  // Title and icon, if provided.
  if (setup->title&&setup->title[0]) {
    xegl_set_title(driver,setup->title);
  }
  if (setup->iconrgba&&(setup->iconw>0)&&(setup->iconh>0)) {
    xegl_set_icon(driver,setup->iconrgba,setup->iconw,setup->iconh);
  }
  
  // Fullscreen if requested.
  if (setup->fullscreen) {
    XChangeProperty(
      DRIVER->dpy,DRIVER->win,
      DRIVER->atom__NET_WM_STATE,
      XA_ATOM,32,PropModeReplace,
      (unsigned char*)&DRIVER->atom__NET_WM_STATE_FULLSCREEN,1
    );
    DRIVER->hdr.fullscreen=1;
  }
  
  // Enable window.
  XMapWindow(DRIVER->dpy,DRIVER->win);
  XSync(DRIVER->dpy,0);
  XSetWMProtocols(DRIVER->dpy,DRIVER->win,&DRIVER->atom_WM_DELETE_WINDOW,1);
  
  // Hide cursor.
  xegl_hide_cursor(driver);
  
  if (xegl_render_init(driver)<0) return -1;
  
  return 0;
}

/* Fullscreen.
 */
 
static void xegl_change_fullscreen(struct pblrt_video *driver,int fullscreen) {
  XEvent evt={
    .xclient={
      .type=ClientMessage,
      .message_type=DRIVER->atom__NET_WM_STATE,
      .send_event=1,
      .format=32,
      .window=DRIVER->win,
      .data={.l={
        fullscreen,
        DRIVER->atom__NET_WM_STATE_FULLSCREEN,
      }},
    }
  };
  XSendEvent(DRIVER->dpy,RootWindow(DRIVER->dpy,DRIVER->screen),0,SubstructureNotifyMask|SubstructureRedirectMask,&evt);
  XFlush(DRIVER->dpy);
}
 
static void xegl_set_fullscreen(struct pblrt_video *driver,int fullscreen) {
  if (fullscreen) {
    if (DRIVER->hdr.fullscreen) return;
    xegl_change_fullscreen(driver,1);
    DRIVER->hdr.fullscreen=1;
  } else {
    if (!DRIVER->hdr.fullscreen) return;
    xegl_change_fullscreen(driver,0);
    DRIVER->hdr.fullscreen=0;
  }
}

/* Suppress screensaver.
 */
 
static void xegl_suppress_screensaver(struct pblrt_video *driver) {
  if (DRIVER->screensaver_suppressed) return;
  DRIVER->screensaver_suppressed=1;
  XForceScreenSaver(DRIVER->dpy,ScreenSaverReset);
}

/* Commit framebuffer.
 */

static int xegl_commit_framebuffer(struct pblrt_video *driver,const void *rgba,int w,int h) {
  eglMakeCurrent(DRIVER->egldisplay,DRIVER->eglsurface,DRIVER->eglsurface,DRIVER->eglcontext);
  if (xegl_render_commit(driver,rgba,w,h)<0) return -1;
  eglSwapBuffers(DRIVER->egldisplay,DRIVER->eglsurface);
  return 0;
}

/* Type definition.
 */
 
const struct pblrt_video_type pblrt_video_type_xegl={
  .name="xegl",
  .desc="X11 with OpenGL, for desktop Linux.",
  .objlen=sizeof(struct pblrt_video_xegl),
  .del=xegl_del,
  .init=xegl_init,
  .update=xegl_update,
  .set_fullscreen=xegl_set_fullscreen,
  .commit_framebuffer=xegl_commit_framebuffer,
  .suppress_screensaver=xegl_suppress_screensaver,
};
