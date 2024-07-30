#include "pblrt_internal.h"

/* Default keyboard map.
 */
 
static int pblrt_inmgr_generate_default_keymap(struct pblrt_inmgr *inmgr) {
  #define BTN(keycode,tag) if (pblrt_inmgr_keymapv_add(inmgr,keycode,PBL_BTN_##tag)<0) return -1;
  #define SIG(keycode,tag) if (pblrt_inmgr_keymapv_add(inmgr,keycode,PBLRT_SIGNAL_##tag)<0) return -1;
  
  // Escape => QUIT
  SIG(0x00070029,QUIT)
  
  // Pause => PAUSE
  SIG(0x00070048,PAUSE)
  
  // F12..F6 => PAUSE,FULLSCREEN,STEP,SCREENCAP,SAVESTATE,LOADSTATE,INCFG
  // So F11 and F12 are similar to how web browsers map them. The rest are willy-nilly.
  SIG(0x00070045,PAUSE)
  SIG(0x00070044,FULLSCREEN)
  SIG(0x00070043,STEP)
  SIG(0x00070042,SCREENCAP)
  SIG(0x00070041,SAVESTATE)
  SIG(0x00070040,LOADSTATE)
  SIG(0x0007003f,INCFG)
  
  // WASD => dpad
  BTN(0x0007001a,UP)
  BTN(0x00070007,RIGHT)
  BTN(0x00070016,DOWN)
  BTN(0x00070004,LEFT)
  
  // Arrows => dpad
  BTN(0x0007004f,RIGHT)
  BTN(0x00070050,LEFT)
  BTN(0x00070051,DOWN)
  BTN(0x00070052,UP)
  
  // Space,comma,dot,slash => thumbs
  BTN(0x0007002c,SOUTH)
  BTN(0x00070036,WEST)
  BTN(0x00070037,EAST)
  BTN(0x00070038,NORTH)
  
  // ZXCV => thumbs
  BTN(0x0007001d,SOUTH)
  BTN(0x0007001b,WEST)
  BTN(0x00070006,EAST)
  BTN(0x00070019,NORTH)
  
  // Tab,backslash,grave,backspace => triggers
  BTN(0x0007002b,L1)
  BTN(0x00070031,R1)
  BTN(0x00070035,L2)
  BTN(0x0007002a,R2)
  
  // Enter,close-bracket,open-bracket => aux
  BTN(0x00070028,AUX1)
  BTN(0x00070030,AUX2)
  BTN(0x0007002f,AUX3)
  
  // KP 45682 => dpad
  BTN(0x0007005a,DOWN)
  BTN(0x0007005c,LEFT)
  BTN(0x0007005d,DOWN)
  BTN(0x0007005e,RIGHT)
  BTN(0x00070060,UP)
  
  // KP 0,enter,plus,dot => thumbs
  BTN(0x00070062,SOUTH)
  BTN(0x00070058,WEST)
  BTN(0x00070057,EAST)
  BTN(0x00070063,NORTH)
  
  // KP 7913 => triggers ('1' on top, '2' on bottom)
  BTN(0x0007005f,L1)
  BTN(0x00070061,R1)
  BTN(0x00070059,L2)
  BTN(0x0007005b,R2)
  
  // KP slash,star,dash => aux
  BTN(0x00070054,AUX1)
  BTN(0x00070055,AUX2)
  BTN(0x00070056,AUX3)
  
  #undef BTN
  #undef SIG
  return 0;
}

/* Init.
 */
 
int pblrt_inmgr_init(struct pblrt_inmgr *inmgr) {
  //TODO Load templates.
  
  // If we didn't acquire a keyboard map, generate a default.
  if (!inmgr->keymapc) {
    if (pblrt_inmgr_generate_default_keymap(inmgr)<0) return -1;
  }
  return 0;
}

/* Cleanup device.
 */
 
void pblrt_inmgr_device_cleanup(struct pblrt_inmgr_device *device) {
  if (device->buttonv) free(device->buttonv);
}

/* Search device list.
 */
 
int pblrt_inmgr_devicev_search(const struct pblrt_inmgr *inmgr,int devid) {
  int lo=0,hi=inmgr->devicec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct pblrt_inmgr_device *q=inmgr->devicev+ck;
         if (devid<q->devid) hi=ck;
    else if (devid>q->devid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Insert to device list.
 */
 
struct pblrt_inmgr_device *pblrt_inmgr_devicev_insert(struct pblrt_inmgr *inmgr,int p,int devid) {
  if ((p<0)||(p>inmgr->devicec)) return 0;
  if (inmgr->devicec>=inmgr->devicea) {
    int na=inmgr->devicea+16;
    if (na>INT_MAX/sizeof(struct pblrt_inmgr_device)) return 0;
    void *nv=realloc(inmgr->devicev,sizeof(struct pblrt_inmgr_device)*na);
    if (!nv) return 0;
    inmgr->devicev=nv;
    inmgr->devicea=na;
  }
  struct pblrt_inmgr_device *device=inmgr->devicev+p;
  memmove(device+1,device,sizeof(struct pblrt_inmgr_device)*(inmgr->devicec-p));
  inmgr->devicec++;
  memset(device,0,sizeof(struct pblrt_inmgr_device));
  device->devid=devid;
  return device;
}

/* Search templates.
 */
 
struct pblrt_inmgr_tm *pblrt_inmgr_tmv_search(const struct pblrt_inmgr *inmgr,int vid,int pid,int version,const char *name) {
  int namec=0;
  if (name) while (name[namec]) namec++;
  struct pblrt_inmgr_tm *tm=inmgr->tmv;
  int i=inmgr->tmc;
  for (;i-->0;tm++) {
    if (tm->vid&&(tm->vid!=vid)) continue;
    if (tm->pid&&(tm->pid!=pid)) continue;
    if (tm->version&&(tm->version!=version)) continue;
    if (tm->namec) {
      if (tm->namec!=namec) continue;
      if (memcmp(tm->name,name,namec)) continue;
    }
    return tm;
  }
  return 0;
}

/* Search device buttons.
 */
 
int pblrt_inmgr_device_buttonv_search(const struct pblrt_inmgr_device *device,int btnid) {
  int lo=0,hi=device->buttonc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct pblrt_inmgr_device_button *q=device->buttonv+ck;
         if (btnid<q->srcbtnid) hi=ck;
    else if (btnid>q->srcbtnid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Add device button.
 */
 
struct pblrt_inmgr_device_button *pblrt_inmgr_device_buttonv_add(struct pblrt_inmgr_device *device,int srcbtnid) {
  int p=pblrt_inmgr_device_buttonv_search(device,srcbtnid);
  if (p>=0) return device->buttonv+srcbtnid;
  p=-p-1;
  if (device->buttonc>=device->buttona) {
    int na=device->buttona+16;
    if (na>INT_MAX/sizeof(struct pblrt_inmgr_device_button)) return 0;
    void *nv=realloc(device->buttonv,sizeof(struct pblrt_inmgr_device_button)*na);
    if (!nv) return 0;
    device->buttonv=nv;
    device->buttona=na;
  }
  struct pblrt_inmgr_device_button *button=device->buttonv+p;
  memmove(button+1,button,sizeof(struct pblrt_inmgr_device_button)*(device->buttonc-p));
  device->buttonc++;
  memset(button,0,sizeof(struct pblrt_inmgr_device_button));
  button->srcbtnid=srcbtnid;
  return button;
}

/* Player ID for newly-connected device.
 * Note that keyboard is always player one and does not participate in this assignment logic.
 */
 
int pblrt_inmgr_next_playerid(const struct pblrt_inmgr *inmgr) {
  int count_by_playerid[1+PBLRT_INPUT_COUNT]={0}; // Include a dummy [0] so we don't have to subtract one every time.
  const struct pblrt_inmgr_device *device=inmgr->devicev;
  int i=inmgr->devicec;
  for (;i-->0;device++) {
    if (device->playerid<1) continue;
    if (device->playerid>PBLRT_INPUT_COUNT) continue;
    count_by_playerid[device->playerid]++;
  }
  int playerid=1,score=count_by_playerid[1];
  for (i=2;i<=PBLRT_INPUT_COUNT;i++) {
    if (count_by_playerid[i]<score) {
      playerid=i;
      score=count_by_playerid[i];
    }
  }
  return playerid;
}

/* Search keyboard maps.
 */
 
int pblrt_inmgr_keymapv_search(const struct pblrt_inmgr *inmgr,int keycode) {
  int lo=0,hi=inmgr->keymapc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct pblrt_inmgr_keymap *q=inmgr->keymapv+ck;
         if (keycode<q->keycode) hi=ck;
    else if (keycode>q->keycode) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Add keyboard map.
 */
 
int pblrt_inmgr_keymapv_add(struct pblrt_inmgr *inmgr,int keycode,int btnid) {
  int p=pblrt_inmgr_keymapv_search(inmgr,keycode);
  if (p>=0) {
    inmgr->keymapv[p].btnid=btnid;
  } else {
    if (inmgr->keymapc>=inmgr->keymapa) {
      int na=inmgr->keymapa+32;
      if (na>INT_MAX/sizeof(struct pblrt_inmgr_keymap)) return -1;
      void *nv=realloc(inmgr->keymapv,sizeof(struct pblrt_inmgr_keymap)*na);
      if (!nv) return -1;
      inmgr->keymapv=nv;
      inmgr->keymapa=na;
    }
    p=-p-1;
    struct pblrt_inmgr_keymap *keymap=inmgr->keymapv+p;
    memmove(keymap+1,keymap,sizeof(struct pblrt_inmgr_keymap)*(inmgr->keymapc-p));
    inmgr->keymapc++;
    keymap->keycode=keycode;
    keymap->btnid=btnid;
  }
  return 0;
}

/* Append to templates.
 */
 
struct pblrt_inmgr_tm *pblrt_inmgr_tm_new(struct pblrt_inmgr *inmgr,int vid,int pid,int version,const char *name) {
  if (inmgr->tmc>=inmgr->tma) {
    int na=inmgr->tma+16;
    if (na>INT_MAX/sizeof(struct pblrt_inmgr_tm)) return 0;
    void *nv=realloc(inmgr->tmv,sizeof(struct pblrt_inmgr_tm)*na);
    if (!nv) return 0;
    inmgr->tmv=nv;
    inmgr->tma=na;
  }
  char *nname=0;
  int namec=0;
  if (name) while (name[namec]) namec++;
  if (namec) {
    if (!(nname=malloc(namec+1))) return 0;
    memcpy(nname,name,namec);
    nname[namec]=0;
  }
  struct pblrt_inmgr_tm *tm=inmgr->tmv+inmgr->tmc++;
  memset(tm,0,sizeof(struct pblrt_inmgr_tm));
  tm->vid=vid;
  tm->pid=pid;
  tm->version=version;
  tm->name=nname;
  tm->namec=namec;
  return tm;
}

/* Remove from templates.
 */

void pblrt_inmgr_tm_del(struct pblrt_inmgr *inmgr,struct pblrt_inmgr_tm *tm) {
  if (!inmgr||!tm) return;
  int p=tm-inmgr->tmv;
  if ((p<0)||(p>=inmgr->tmc)) return;
  if (tm->name) free(tm->name);
  if (tm->buttonv) free(tm->buttonv);
  inmgr->tmc--;
  memmove(tm,tm+1,sizeof(struct pblrt_inmgr_tm)*(inmgr->tmc-p));
}

/* Search buttons in template.
 */
 
int pblrt_inmgr_tm_buttonv_search(const struct pblrt_inmgr_tm *tm,int srcbtnid) {
  int lo=0,hi=tm->buttonc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct pblrt_inmgr_tm_button *q=tm->buttonv+ck;
         if (srcbtnid<q->srcbtnid) hi=ck;
    else if (srcbtnid>q->srcbtnid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Add button to template.
 */
 
struct pblrt_inmgr_tm_button *pblrt_inmgr_tm_add_button(struct pblrt_inmgr_tm *tm,int srcbtnid,int dstbtnid) {
  int p=pblrt_inmgr_tm_buttonv_search(tm,srcbtnid);
  if (p>=0) {
    tm->buttonv[p].dstbtnid=dstbtnid;
    return tm->buttonv+p;
  }
  p=-p-1;
  if (tm->buttonc>=tm->buttona) {
    int na=tm->buttona+16;
    if (na>INT_MAX/sizeof(struct pblrt_inmgr_tm_button)) return 0;
    void *nv=realloc(tm->buttonv,sizeof(struct pblrt_inmgr_tm_button)*na);
    if (!nv) return 0;
    tm->buttonv=nv;
    tm->buttona=na;
  }
  struct pblrt_inmgr_tm_button *button=tm->buttonv+p;
  memmove(button+1,button,sizeof(struct pblrt_inmgr_tm_button)*(tm->buttonc-p));
  tm->buttonc++;
  button->srcbtnid=srcbtnid;
  button->dstbtnid=dstbtnid;
  return button;
}
