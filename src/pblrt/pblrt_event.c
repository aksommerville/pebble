#include "pblrt_internal.h"

/* Driver events.
 */

void pblrt_cb_close() {
  pblrt.terminate++;
}

void pblrt_cb_focus(int focus) {
  if (!focus) {
    if (!pblrt.hardpause) {
      pblrt.hardpause=1;
      pblrt_audio_play(0);
    }
  } else {
    if (pblrt.hardpause) {
      pblrt.hardpause=0;
      pblrt_audio_play(1);
    }
  }
}

void pblrt_cb_key(int keycode,int value) {
  pblrt_inmgr_key(&pblrt.inmgr,keycode,value);
}

void pblrt_cb_pcm(int16_t *v,int c) {
  // Copy from (pblrt.abuf).
  while ((c>0)&&(pblrt.abufc>0)) {
    int cpc=pblrt.abufa-pblrt.abufp;
    if (cpc>pblrt.abufc) cpc=pblrt.abufc;
    if (cpc>c) cpc=c;
    memcpy(v,pblrt.abuf+pblrt.abufp,cpc<<1);
    pblrt.abufc-=cpc;
    if ((pblrt.abufp+=cpc)>=pblrt.abufa) pblrt.abufp=0;
    v+=cpc;
    c-=cpc;
  }
  // Anything we didn't reach, zero it.
  if (c>0) memset(v,0,c<<1);
}

void pblrt_cb_connect(int devid) {
  pblrt_inmgr_add_device(&pblrt.inmgr,devid);
}

void pblrt_cb_disconnect(int devid) {
  pblrt_inmgr_remove_device(&pblrt.inmgr,devid);
}

void pblrt_cb_button(int devid,int btnid,int value) {
  pblrt_inmgr_event(&pblrt.inmgr,devid,btnid,value);
}
