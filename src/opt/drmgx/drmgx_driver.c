#include "drmgx_internal.h"

static int drmgx_swap_egl(struct pblrt_video *driver,uint32_t *fbid);

/* Cleanup.
 */
 
static void drmgx_fb_cleanup(struct pblrt_video *driver,struct drmgx_fb *fb) {
  if (fb->fbid) {
    drmModeRmFB(DRIVER->fd,fb->fbid);
  }
}
 
static void _drmgx_del(struct pblrt_video *driver) {

  // If waiting for a page flip, we must let it finish first.
  if ((DRIVER->fd>=0)&&DRIVER->flip_pending) {
    struct pollfd pollfd={.fd=DRIVER->fd,.events=POLLIN|POLLERR|POLLHUP};
    if (poll(&pollfd,1,500)>0) {
      char dummy[64];
      if (read(DRIVER->fd,dummy,sizeof(dummy))) ;
    }
  }
  
  drmgx_render_cleanup(driver);
  if (DRIVER->eglcontext) eglDestroyContext(DRIVER->egldisplay,DRIVER->eglcontext);
  if (DRIVER->eglsurface) eglDestroySurface(DRIVER->egldisplay,DRIVER->eglsurface);
  if (DRIVER->egldisplay) eglTerminate(DRIVER->egldisplay);
  
  drmgx_fb_cleanup(driver,DRIVER->fbv+0);
  drmgx_fb_cleanup(driver,DRIVER->fbv+1);
  
  if (DRIVER->crtc_restore&&(DRIVER->fd>=0)) {
    drmModeCrtcPtr crtc=DRIVER->crtc_restore;
    drmModeSetCrtc(
      DRIVER->fd,crtc->crtc_id,crtc->buffer_id,
      crtc->x,crtc->y,&DRIVER->connid,1,&crtc->mode
    );
    drmModeFreeCrtc(DRIVER->crtc_restore);
  }
  
  if (DRIVER->fd>=0) close(DRIVER->fd);
}

/* Init.
 */
 
static int _drmgx_init(struct pblrt_video *driver,const struct pblrt_video_setup *setup) {

  DRIVER->fd=-1;
  DRIVER->crtcunset=1;

  if (!drmAvailable()) {
    fprintf(stderr,"DRM not available.\n");
    return -1;
  }
  
  if (drmgx_open_file(driver,setup->device)<0) return -1;
  if (drmgx_configure(driver)<0) return -1;
  if (drmgx_init_gx(driver)<0) return -1;
  if (drmgx_render_init(driver)<0) return -1;
  
  return 0;
}

/* Poll file.
 */
 
static void drmgx_cb_vblank(
  int fd,unsigned int seq,unsigned int times,unsigned int timeus,void *userdata
) {}
 
static void drmgx_cb_page1(
  int fd,unsigned int seq,unsigned int times,unsigned int timeus,void *userdata
) {
  struct pblrt_video *driver=userdata;
  DRIVER->flip_pending=0;
}
 
static void drmgx_cb_page2(
  int fd,unsigned int seq,unsigned int times,unsigned int timeus,unsigned int ctrcid,void *userdata
) {
  drmgx_cb_page1(fd,seq,times,timeus,userdata);
}
 
static void drmgx_cb_seq(
  int fd,uint64_t seq,uint64_t timeus,uint64_t userdata
) {}
 
static int drmgx_poll_file(struct pblrt_video *driver,int to_ms) {
  struct pollfd pollfd={.fd=DRIVER->fd,.events=POLLIN};
  if (poll(&pollfd,1,to_ms)<=0) return 0;
  drmEventContext ctx={
    .version=DRM_EVENT_CONTEXT_VERSION,
    .vblank_handler=drmgx_cb_vblank,
    .page_flip_handler=drmgx_cb_page1,
    .page_flip_handler2=drmgx_cb_page2,
    .sequence_handler=drmgx_cb_seq,
  };
  int err=drmHandleEvent(DRIVER->fd,&ctx);
  if (err<0) return -1;
  return 0;
}

/* Swap.
 */
 
static int drmgx_swap_egl(struct pblrt_video *driver,uint32_t *fbid) {
  eglSwapBuffers(DRIVER->egldisplay,DRIVER->eglsurface);
  
  struct gbm_bo *bo=gbm_surface_lock_front_buffer(DRIVER->gbmsurface);
  if (!bo) return -1;
  
  int handle=gbm_bo_get_handle(bo).u32;
  struct drmgx_fb *fb;
  if (!DRIVER->fbv[0].handle) {
    fb=DRIVER->fbv;
  } else if (handle==DRIVER->fbv[0].handle) {
    fb=DRIVER->fbv;
  } else {
    fb=DRIVER->fbv+1;
  }
  
  if (!fb->fbid) {
    int width=gbm_bo_get_width(bo);
    int height=gbm_bo_get_height(bo);
    int stride=gbm_bo_get_stride(bo);
    fb->handle=handle;
    if (drmModeAddFB(DRIVER->fd,width,height,24,32,stride,fb->handle,&fb->fbid)<0) return -1;
    
    if (DRIVER->crtcunset) {
      if (drmModeSetCrtc(
        DRIVER->fd,DRIVER->crtcid,fb->fbid,0,0,
        &DRIVER->connid,1,&DRIVER->mode
      )<0) {
        fprintf(stderr,"drmModeSetCrtc: %m\n");
        return -1;
      }
      DRIVER->crtcunset=0;
    }
  }
  
  *fbid=fb->fbid;
  if (DRIVER->bo_pending) {
    gbm_surface_release_buffer(DRIVER->gbmsurface,DRIVER->bo_pending);
  }
  DRIVER->bo_pending=bo;
  
  return 0;
}

static int drmgx_swap(struct pblrt_video *driver) {

  // There must be no more than one page flip in flight at a time.
  // If one is pending -- likely -- give it a chance to finish.
  if (DRIVER->flip_pending) {
    if (drmgx_poll_file(driver,20)<0) return -1;
    if (DRIVER->flip_pending) {
      // Page flip didn't complete after a 20 ms delay? Drop the frame, no worries.
      return 0;
    }
  }
  DRIVER->flip_pending=1;
  
  uint32_t fbid=0;
  if (drmgx_swap_egl(driver,&fbid)<0) {
    DRIVER->flip_pending=0;
    return -1;
  }
  
  if (drmModePageFlip(DRIVER->fd,DRIVER->crtcid,fbid,DRM_MODE_PAGE_FLIP_EVENT,driver)<0) {
    fprintf(stderr,"drmModePageFlip: %m\n");
    DRIVER->flip_pending=0;
    return -1;
  }

  return 0;
}

/* Commit framebuffer.
 */
 
static int _drmgx_commit_framebuffer(struct pblrt_video *driver,const void *fb,int fbw,int fbh) {
  if (drmgx_render_commit(driver,fb,fbw,fbh)<0) return -1;
  return drmgx_swap(driver);
}

/* Type definition.
 */
 
const struct pblrt_video_type pblrt_video_type_drmgx={
  .name="drmgx",
  .desc="Direct rendering, for Linux systems with no desktop environment.",
  .objlen=sizeof(struct pblrt_video_drmgx),
  .appointment_only=0,
  .del=_drmgx_del,
  .init=_drmgx_init,
  .commit_framebuffer=_drmgx_commit_framebuffer,
};
