/* pblrt_inmgr.h
 * Input Manager.
 */
 
#ifndef PBLRT_INMGR_H
#define PBLRT_INMGR_H

#include <termios.h>

/* Fake input buttons that operate as stateless signals.
 * These are only valid for TWOSTATE buttons and keyboard keys.
 */
#define PBLRT_SIGNAL_QUIT          0x00010001
#define PBLRT_SIGNAL_PAUSE         0x00010002 /* toggle */
#define PBLRT_SIGNAL_FULLSCREEN    0x00010003 /* toggle */
#define PBLRT_SIGNAL_STEP          0x00010004
#define PBLRT_SIGNAL_SCREENCAP     0x00010005
#define PBLRT_SIGNAL_SAVESTATE     0x00010006
#define PBLRT_SIGNAL_LOADSTATE     0x00010007
#define PBLRT_SIGNAL_INCFG         0x00010008
#define PBLRT_SIGNAL_FOR_EACH \
  _(QUIT) \
  _(PAUSE) \
  _(FULLSCREEN) \
  _(STEP) \
  _(SCREENCAP) \
  _(SAVESTATE) \
  _(LOADSTATE) \
  _(INCFG)

/* THREEWAY and HAT mappings can only target the dpad.
 */
#define PBLRT_BTNMODE_TWOSTATE 1 /* (srclo..srchi)=1 */
#define PBLRT_BTNMODE_THREEWAY 2 /* <=srclo=LEFT|UP, >=srchi=RIGHT|DOWN. If (srclo>srchi), reverse the assignments. */
#define PBLRT_BTNMODE_HAT      3 /* (srclo==srchi-7) */

struct pblrt_inmgr_device_button {
  int srcbtnid;
  int srcvalue;
  int mode;
  int srclo,srchi;
  int dstbtnid;
  int dstvalue;
};

struct pblrt_inmgr_device {
  int devid;
  int playerid; // Zero until the first significant event, then 1..4.
  int state;
  struct pblrt_inmgr_device_button *buttonv;
  int buttonc,buttona;
};

struct pblrt_inmgr_tm_button {
  int srcbtnid;
  int dstbtnid; // (LEFT|RIGHT) or (UP|DOWN) for THREEWAY, (LEFT|RIGHT|UP|DOWN) for HAT, >=0x10000 for signals.
};

struct pblrt_inmgr_tm {
  int vid,pid,version; // Zero matches all.
  char *name; // Empty matches all, otherwise exact match.
  int namec;
  struct pblrt_inmgr_tm_button *buttonv;
  int buttonc,buttona;
};

struct pblrt_inmgr_keymap {
  int keycode;
  int btnid;
};

struct pblrt_inmgr {
  struct pblrt_inmgr_device *devicev;
  int devicec,devicea;
  struct pblrt_inmgr_tm *tmv; // Order matters. First match wins.
  int tmc,tma;
  struct pblrt_inmgr_keymap *keymapv;
  int keymapc,keymapa;
  struct termios tios_restore;
  int got_stdin;
};

int pblrt_inmgr_init(struct pblrt_inmgr *inmgr);

int pblrt_inmgr_update(struct pblrt_inmgr *inmgr);

int pblrt_inmgr_add_device(struct pblrt_inmgr *inmgr,int devid);
int pblrt_inmgr_remove_device(struct pblrt_inmgr *inmgr,int devid);
int pblrt_inmgr_event(struct pblrt_inmgr *inmgr,int devid,int btnid,int value);
int pblrt_inmgr_key(struct pblrt_inmgr *inmgr,int keycode,int value);

void pblrt_inmgr_device_cleanup(struct pblrt_inmgr_device *device);
int pblrt_inmgr_devicev_search(const struct pblrt_inmgr *inmgr,int devid);
struct pblrt_inmgr_device *pblrt_inmgr_devicev_insert(struct pblrt_inmgr *inmgr,int p,int devid);
struct pblrt_inmgr_tm *pblrt_inmgr_tmv_search(const struct pblrt_inmgr *inmgr,int vid,int pid,int version,const char *name);
struct pblrt_inmgr_tm *pblrt_inmgr_tmv_synthesize(struct pblrt_inmgr *inmgr,int vid,int pid,int version,const char *name,int devid);
int pblrt_inmgr_device_apply_tm(struct pblrt_inmgr *inmgr,struct pblrt_inmgr_device *device,const struct pblrt_inmgr_tm *tm);
int pblrt_inmgr_device_buttonv_search(const struct pblrt_inmgr_device *device,int btnid);
struct pblrt_inmgr_device_button *pblrt_inmgr_device_buttonv_add(struct pblrt_inmgr_device *device,int srcbtnid);
int pblrt_inmgr_next_playerid(const struct pblrt_inmgr *inmgr);
int pblrt_inmgr_signal(struct pblrt_inmgr *inmgr,int btnid);
int pblrt_inmgr_keymapv_search(const struct pblrt_inmgr *inmgr,int keycode);
int pblrt_inmgr_keymapv_add(struct pblrt_inmgr *inmgr,int keycode,int btnid);
struct pblrt_inmgr_tm *pblrt_inmgr_tm_new(struct pblrt_inmgr *inmgr,int vid,int pid,int version,const char *name);
void pblrt_inmgr_tm_del(struct pblrt_inmgr *inmgr,struct pblrt_inmgr_tm *tm);
int pblrt_inmgr_tm_buttonv_search(const struct pblrt_inmgr_tm *tm,int srcbtnid);
struct pblrt_inmgr_tm_button *pblrt_inmgr_tm_add_button(struct pblrt_inmgr_tm *tm,int srcbtnid,int dstbtnid);

#endif
