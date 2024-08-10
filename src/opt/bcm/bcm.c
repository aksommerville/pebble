#include "bcm_internal.h"

/* Quit.
 */

static void _bcm_del(struct pblrt_video *driver) {
  #if 0 /* shits, giggles */
  eglMakeCurrent(DRIVER->egldisplay,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
  eglDestroySurface(DRIVER->egldisplay,DRIVER->eglsurface);
  eglTerminate(DRIVER->egldisplay);
  eglReleaseThread();
  #endif
  bcm_host_deinit();
  if (DRIVER->fb) free(DRIVER->fb);
}

/* Vsync.
 */
 
static void _bcm_cb_vsync(DISPMANX_UPDATE_HANDLE_T update,void *arg) {
  struct pblrt_video *driver=arg;
  DRIVER->vsync_seq++;
}

/* Init.
 */

static int _bcm_init(struct pblrt_video *driver,const struct pblrt_video_setup *setup) {

  bcm_host_init();

  // We enforce a screen size sanity limit of 4096. Could be as high as 32767 if we felt like it.
  int screenw,screenh;
  graphics_get_display_size(0,&screenw,&screenh);
  if ((screenw<1)||(screenh<1)) return -1;
  if ((screenw>4096)||(screenh>4096)) return -1;

  if (!(DRIVER->vcdisplay=vc_dispmanx_display_open(0))) return -1;
  if (!(DRIVER->vcupdate=vc_dispmanx_update_start(0))) return -1;

  int logw=screenw-80;
  int logh=screenh-50;
  VC_RECT_T srcr={0,0,screenw<<16,screenh<<16};
  VC_RECT_T dstr={(screenw>>1)-(logw>>1),(screenh>>1)-(logh>>1),logw,logh};

  DRIVER->fbw=setup->fbw;
  DRIVER->fbh=setup->fbh;
  if (!(DRIVER->fb=malloc(setup->fbw*setup->fbh*4))) return -1;
  if (!(DRIVER->vcresource=vc_dispmanx_resource_create(
    VC_IMAGE_XBGR8888,setup->fbw,setup->fbh,DRIVER->fb
  ))) return -1;

  VC_DISPMANX_ALPHA_T alpha={DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS,0xffffffff};
  if (!(DRIVER->vcelement=vc_dispmanx_element_add(
    DRIVER->vcupdate,DRIVER->vcdisplay,1,&dstr,DRIVER->vcresource,&srcr,DISPMANX_PROTECTION_NONE,&alpha,0,0
  ))) return -1;

  if (vc_dispmanx_update_submit_sync(DRIVER->vcupdate)<0) return -1;

  if (vc_dispmanx_vsync_callback(DRIVER->vcdisplay,_bcm_cb_vsync,driver)<0) return -1;

  #if 0 /* just for shits and giggles, i want to try with a raw framebuffer first */
  VC_DISPMANX_ALPHA_T alpha={DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS,0xffffffff};
  if (!(DRIVER->vcelement=vc_dispmanx_element_add(
    DRIVER->vcupdate,DRIVER->vcdisplay,1,&dstr,0,&srcr,DISPMANX_PROTECTION_NONE,&alpha,0,0
  ))) return -1;

  DRIVER->eglwindow.element=DRIVER->vcelement;
  DRIVER->eglwindow.width=screenw;
  DRIVER->eglwindow.height=screenh;
  if (vc_dispmanx_update_submit_sync(DRIVER->vcupdate)<0) return -1;
  static const EGLint eglattr[]={
    EGL_RED_SIZE,8,
    EGL_GREEN_SIZE,8,
    EGL_BLUE_SIZE,8,
    EGL_ALPHA_SIZE,0,
    EGL_DEPTH_SIZE,0,
    EGL_LUMINANCE_SIZE,EGL_DONT_CARE,
    EGL_SURFACE_TYPE,EGL_WINDOW_BIT,
    EGL_SAMPLES,1,
  EGL_NONE};
  static EGLint ctxattr[]={
    EGL_CONTEXT_CLIENT_VERSION,2,
  EGL_NONE};

  DRIVER->egldisplay=eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (eglGetError()!=EGL_SUCCESS) return -1;

  eglInitialize(DRIVER->egldisplay,0,0);
  if (eglGetError()!=EGL_SUCCESS) return -1;

  eglBindAPI(EGL_OPENGL_ES_API);

  EGLint configc=0;
  eglChooseConfig(DRIVER->egldisplay,eglattr,&DRIVER->eglconfig,1,&configc);
  if (eglGetError()!=EGL_SUCCESS) return -1;
  if (configc<1) return -1;

  DRIVER->eglsurface=eglCreateWindowSurface(DRIVER->egldisplay,DRIVER->eglconfig,&DRIVER->eglwindow,0);
  if (eglGetError()!=EGL_SUCCESS) return -1;

  DRIVER->eglcontext=eglCreateContext(DRIVER->egldisplay,DRIVER->eglconfig,0,ctxattr);
  if (eglGetError()!=EGL_SUCCESS) return -1;

  eglMakeCurrent(DRIVER->egldisplay,DRIVER->eglsurface,DRIVER->eglsurface,DRIVER->eglcontext);
  if (eglGetError()!=EGL_SUCCESS) return -1;

  eglSwapInterval(DRIVER->egldisplay,1);
  #endif

  return 0;
}

/* Swap buffers.
 */

static int _bcm_commit_framebuffer(struct pblrt_video *driver,const void *fb,int fbw,int fbh) {
  if (!fb||(fbw!=DRIVER->fbw)||(fbh!=DRIVER->fbh)) return 0;
  int seq=DRIVER->vsync_seq;
  //memcpy(DRIVER->fb,fb,fbw*fbh*4);
  VC_RECT_T fbr={0,0,fbw,fbh};
  vc_dispmanx_resource_write_data(DRIVER->vcresource,VC_IMAGE_XBGR8888,fbw<<2,fb,&fbr);
  int panic=100;
  while ((DRIVER->vsync_seq==seq)&&(panic-->0)) {
    usleep(1000);
  }
  //eglSwapBuffers(DRIVER->egldisplay,DRIVER->eglsurface);
  return 0;
}

/* Type definition.
 */
 
const struct pblrt_video_type pblrt_video_type_bcm={
  .name="bcm",
  .desc="Broadcom VideoCore, for early Raspberry Pi. Prefer 'drmgx' if it works.",
  .objlen=sizeof(struct pblrt_video_bcm),
  .del=_bcm_del,
  .init=_bcm_init,
  .commit_framebuffer=_bcm_commit_framebuffer,
};
