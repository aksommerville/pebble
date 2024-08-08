#include "pblrt_internal.h"
#include <sys/poll.h>
#include <unistd.h>

/* Dispatch signal.
 */
 
int pblrt_inmgr_signal(struct pblrt_inmgr *inmgr,int btnid) {
  switch (btnid) {
    case PBLRT_SIGNAL_QUIT: pblrt.terminate++; break;
    case PBLRT_SIGNAL_PAUSE: break; // TODO
    case PBLRT_SIGNAL_FULLSCREEN: break; // TODO
    case PBLRT_SIGNAL_STEP: break; // TODO
    case PBLRT_SIGNAL_SCREENCAP: break; // TODO
    case PBLRT_SIGNAL_SAVESTATE: break; // TODO
    case PBLRT_SIGNAL_LOADSTATE: break; // TODO
    case PBLRT_SIGNAL_INCFG: break; // TODO
  }
  return 0;
}

/* Device connected.
 */
 
int pblrt_inmgr_add_device(struct pblrt_inmgr *inmgr,int devid) {
  if (!pblrt.input) return -1;
  int p=pblrt_inmgr_devicev_search(inmgr,devid);
  if (p>=0) return -1;
  p=-p-1;
  struct pblrt_inmgr_device *device=pblrt_inmgr_devicev_insert(inmgr,p,devid);
  if (!device) return -1;

  if (!pblrt.input->type->ids_by_devid) return 0; // Leave the device here but unconfigured.
  int vid=0,pid=0,version=0;
  const char *name=pblrt.input->type->ids_by_devid(&vid,&pid,&version,pblrt.input,devid);

  const struct pblrt_inmgr_tm *tm=pblrt_inmgr_tmv_search(inmgr,vid,pid,version,name);
  if (!tm) {
    if (!(tm=pblrt_inmgr_tmv_synthesize(inmgr,vid,pid,version,name,devid))) {
      fprintf(stderr,"%s: Failed to generate default mapping for device '%s' (%04x:%04x:%04x)\n",pblrt.exename,name,vid,pid,version);
      return -2;
    } else {
      fprintf(stderr,"%s: Generated mapping for device '%s' (%04x:%04x:%04x)\n",pblrt.exename,name,vid,pid,version);
    }
  }
  int err=pblrt_inmgr_device_apply_tm(inmgr,device,tm);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error applying mapping for device '%s' (%04x:%04x:%04x)\n",pblrt.exename,name,vid,pid,version);
    return -2;
  }
  device->state|=PBL_BTN_CD;
  fprintf(stderr,"%s: Connected input device '%s' (%04x:%04x:%04x)\n",pblrt.exename,name,vid,pid,version);
  return 0;
}

/* Device disconnected.
 */

int pblrt_inmgr_remove_device(struct pblrt_inmgr *inmgr,int devid) {
  int p=pblrt_inmgr_devicev_search(inmgr,devid);
  if (p<0) return 0;
  struct pblrt_inmgr_device *device=inmgr->devicev+p;
  
  // Drop this device's state from the global, but keep CD on if anyone else maps to this playerid.
  if ((device->playerid>=1)&&(device->playerid<=PBLRT_INPUT_COUNT)) {
    pblrt.instate[device->playerid-1]&=~(device->state|PBL_BTN_CD);
    const struct pblrt_inmgr_device *other=inmgr->devicev;
    int i=inmgr->devicec;
    for (;i-->0;other++) {
      if (other==device) continue;
      if (other->playerid!=device->playerid) continue;
      pblrt.instate[device->playerid-1]|=PBL_BTN_CD;
      break;
    }
  }
  
  pblrt_inmgr_device_cleanup(device);
  inmgr->devicec--;
  memmove(device,device+1,sizeof(struct pblrt_inmgr_device)*(inmgr->devicec-p));
  return 0;
}

/* Event from enumerated device.
 */
 
int pblrt_inmgr_event(struct pblrt_inmgr *inmgr,int devid,int btnid,int value) {
  //fprintf(stderr,"%s %d.%d=%d\n",__func__,devid,btnid,value);
  int p=pblrt_inmgr_devicev_search(inmgr,devid);
  if (p<0) return 0;
  struct pblrt_inmgr_device *device=inmgr->devicev+p;
  p=pblrt_inmgr_device_buttonv_search(device,btnid);
  if (p<0) return 0;
  struct pblrt_inmgr_device_button *button=device->buttonv+p;
  if (value==button->srcvalue) return 0;
  button->srcvalue=value;
  int mask=0,state=0;
  switch (button->mode) {
  
    case PBLRT_BTNMODE_TWOSTATE: {
        int dstvalue=((value>=button->srclo)&&(value<=button->srchi))?1:0;
        if (button->dstvalue==dstvalue) return 0;
        button->dstvalue=dstvalue;
        mask=button->dstbtnid;
        if (dstvalue) {
          state=mask;
          if (button->dstbtnid>=0x10000) return pblrt_inmgr_signal(inmgr,button->dstbtnid);
        }
      } break;
      
    case PBLRT_BTNMODE_THREEWAY: {
        int dstvalue;
        if (button->srclo<button->srchi) {
          dstvalue=(value<=button->srclo)?-1:(value>=button->srchi)?1:0;
        } else {
          dstvalue=(value<=button->srchi)?1:(value>=button->srclo)?-1:0;
        }
        if (button->dstvalue==dstvalue) return 0;
        button->dstvalue=dstvalue;
        mask=button->dstbtnid;
        if (dstvalue<0) {
          state=(PBL_BTN_LEFT|PBL_BTN_UP)&mask;
        } else if (dstvalue>0) {
          state=(PBL_BTN_RIGHT|PBL_BTN_DOWN)&mask;
        }
      } break;
      
    case PBLRT_BTNMODE_HAT: {
        int pdx=0,pdy=0,ndx=0,ndy=0;
        switch (button->dstvalue) {
          case 1: case 2: case 3: pdx=1; break;
          case 5: case 6: case 7: pdx=-1; break;
        }
        switch (button->dstvalue) {
          case 7: case 0: case 1: pdy=-1; break;
          case 3: case 4: case 5: pdy=1; break;
        }
        button->dstvalue=value-button->srclo;
        switch (button->dstvalue) {
          case 1: case 2: case 3: ndx=1; break;
          case 5: case 6: case 7: ndx=-1; break;
        }
        switch (button->dstvalue) {
          case 7: case 0: case 1: ndy=-1; break;
          case 3: case 4: case 5: ndy=1; break;
        }
        if ((pdx==ndx)&&(pdy==ndy)) return 0;
        mask=PBL_BTN_LEFT|PBL_BTN_RIGHT|PBL_BTN_UP|PBL_BTN_DOWN;
        if (ndx<0) state|=PBL_BTN_LEFT;
        else if (ndx>0) state|=PBL_BTN_RIGHT;
        if (ndy<0) state|=PBL_BTN_UP;
        else if (ndy>0) state|=PBL_BTN_DOWN;
      } break;
      
  }
  if (!(mask&0xffff)) return 0;
  if ((device->state&mask)==state) return 0;
  device->state|=PBL_BTN_CD;
  device->state=(device->state&~mask)|state;
  if (!device->playerid) {
    device->playerid=pblrt_inmgr_next_playerid(inmgr);
  }
  if ((device->playerid>=1)&&(device->playerid<=PBLRT_INPUT_COUNT)) {
    pblrt.instate[device->playerid-1]=(pblrt.instate[device->playerid-1]&~mask)|state;
  }
  return 0;
}

/* Event from system keyboard.
 */
 
int pblrt_inmgr_key(struct pblrt_inmgr *inmgr,int keycode,int value) {
  int p=pblrt_inmgr_keymapv_search(inmgr,keycode);
  if (p<0) return 0;
  struct pblrt_inmgr_keymap *keymap=inmgr->keymapv+p;
  if (keymap->btnid>=0x10000) {
    if (!value) return 0;
    return pblrt_inmgr_signal(inmgr,keymap->btnid);
  }
  if (value) {
    if (pblrt.instate[0]&keymap->btnid) return 0;
    pblrt.instate[0]|=keymap->btnid;
    pblrt.instate[0]|=PBL_BTN_CD;
  } else {
    if (!(pblrt.instate[0]&keymap->btnid)) return 0;
    pblrt.instate[0]&=~keymap->btnid;
  }
  return 0;
}

/* Update.
 * The real meat-and-potatoes of what we do is driven by event callbacks.
 * This update is only for polling stdin -- it's possible that we're not getting events from anywhere else,
 * and there must always be a way to quit.
 */
 
int pblrt_inmgr_update(struct pblrt_inmgr *inmgr) {
  if (!inmgr->got_stdin) return 0;
  struct pollfd pollfd={.fd=STDIN_FILENO,.events=POLLIN};
  if (poll(&pollfd,1,0)<=0) return 0;
  uint8_t buf[64];
  int bufc=read(STDIN_FILENO,buf,sizeof(buf));
  if (bufc<=0) return 0;
  if ((bufc==1)&&(buf[0]==0x1b)) {
    pblrt.terminate=1;
  }
  return 0;
}
