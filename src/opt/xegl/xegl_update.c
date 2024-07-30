#include "xegl_internal.h"

/* Key press, release, or repeat.
 */
 
static int xegl_evt_key(struct pblrt_video *driver,XKeyEvent *evt,int value) {
  if (driver->delegate.cb_key) {
    KeySym keysym=XkbKeycodeToKeysym(DRIVER->dpy,evt->keycode,0,0);
    if (keysym) {
      int keycode=xegl_usb_usage_from_keysym((int)keysym);
      if (keycode) {
        driver->delegate.cb_key(keycode,value);
      }
    }
  }
  return 0;
}

/* Client message.
 */
 
static int xegl_evt_client(struct pblrt_video *driver,XClientMessageEvent *evt) {
  if (evt->message_type==DRIVER->atom_WM_PROTOCOLS) {
    if (evt->format==32) {
      if (evt->data.l[0]==DRIVER->atom_WM_DELETE_WINDOW) {
        if (driver->delegate.cb_close) {
          driver->delegate.cb_close();
        }
      }
    }
  }
  return 0;
}

/* Configuration event (eg resize).
 */
 
static int xegl_evt_configure(struct pblrt_video *driver,XConfigureEvent *evt) {
  int nw=evt->width,nh=evt->height;
  if ((nw!=DRIVER->hdr.w)||(nh!=DRIVER->hdr.h)) {
    DRIVER->hdr.w=nw;
    DRIVER->hdr.h=nh;
  }
  return 0;
}

/* Focus.
 */
 
static int xegl_evt_focus(struct pblrt_video *driver,XFocusInEvent *evt,int value) {
  if (value==DRIVER->focus) return 0;
  DRIVER->focus=value;
  if (driver->delegate.cb_focus) {
    driver->delegate.cb_focus(value);
  }
  return 0;
}

/* Process one event.
 */
 
static int xegl_receive_event(struct pblrt_video *driver,XEvent *evt) {
  if (!evt) return -1;
  switch (evt->type) {
  
    case KeyPress: return xegl_evt_key(driver,&evt->xkey,1);
    case KeyRelease: return xegl_evt_key(driver,&evt->xkey,0);
    case KeyRepeat: return xegl_evt_key(driver,&evt->xkey,2);
    
    case ClientMessage: return xegl_evt_client(driver,&evt->xclient);
    
    case ConfigureNotify: return xegl_evt_configure(driver,&evt->xconfigure);
    
    case FocusIn: return xegl_evt_focus(driver,&evt->xfocus,1);
    case FocusOut: return xegl_evt_focus(driver,&evt->xfocus,0);
    
  }
  return 0;
}

/* Update.
 */
 
int xegl_update(struct pblrt_video *driver) {
  int evtc=XEventsQueued(DRIVER->dpy,QueuedAfterFlush);
  while (evtc-->0) {
    XEvent evt={0};
    XNextEvent(DRIVER->dpy,&evt);
    if ((evtc>0)&&(evt.type==KeyRelease)) {
      XEvent next={0};
      XNextEvent(DRIVER->dpy,&next);
      evtc--;
      if ((next.type==KeyPress)&&(evt.xkey.keycode==next.xkey.keycode)&&(evt.xkey.time>=next.xkey.time-XEGL_KEY_REPEAT_INTERVAL)) {
        evt.type=KeyRepeat;
        if (xegl_receive_event(driver,&evt)<0) return -1;
      } else {
        if (xegl_receive_event(driver,&evt)<0) return -1;
        if (xegl_receive_event(driver,&next)<0) return -1;
      }
    } else {
      if (xegl_receive_event(driver,&evt)<0) return -1;
    }
  }
  return 0;
}
