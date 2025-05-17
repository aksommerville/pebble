#include "pblrt_internal.h"

/* Choose button ID when we're guided only by the range.
 * So, pick the least popular thing within some set.
 */
 
static int pblrt_inmgr_tm_least_popular_axis(const struct pblrt_inmgr_tm *tm) {
  int lc=0,rc=0,uc=0,dc=0,xc=0,yc=0;
  const struct pblrt_inmgr_tm_button *button=tm->buttonv;
  int i=tm->buttonc;
  for (;i-->0;button++) {
    switch (button->dstbtnid) {
      case PBL_BTN_LEFT: lc++; break;
      case PBL_BTN_RIGHT: rc++; break;
      case PBL_BTN_UP: uc++; break;
      case PBL_BTN_DOWN: dc++; break;
      case PBL_BTN_LEFT|PBL_BTN_RIGHT: xc++; break;
      case PBL_BTN_UP|PBL_BTN_DOWN: yc++; break;
    }
  }
  if (lc<rc) xc+=lc; else xc+=rc;
  if (uc<dc) yc+=uc; else uc+=dc;
  if (xc<=yc) return PBL_BTN_LEFT|PBL_BTN_RIGHT;
  return PBL_BTN_UP|PBL_BTN_DOWN;
}

static int pblrt_inmgr_tm_least_popular_bit(const struct pblrt_inmgr_tm *tm,int mask) {
  if (!(mask&=0xffff)) return 0;
  int count_by_index[16]={0};
  const struct pblrt_inmgr_tm_button *button=tm->buttonv;
  int i=tm->buttonc;
  for (;i-->0;button++) {
    if (!(button->dstbtnid&mask)) continue;
    uint16_t tmp=button->dstbtnid;
    int index=0;
    for (;tmp;tmp>>=1,index++) {
      if (tmp&1) count_by_index[index]++;
    }
  }
  int btnid=0,score=999;
  uint16_t tmp=mask;
  int index=0;
  for (;tmp;tmp>>=1,index++) {
    if (!(tmp&1)) continue;
    if (count_by_index[index]<score) {
      btnid=1<<index;
      score=count_by_index[index];
    }
  }
  return btnid;
}

/* Choose a button ID, when the driver didn't express a preference.
 */
 
static int pblrt_inmgr_guess_btnid(int btnid,int hidusage,int lo,int hi,int rest,const struct pblrt_inmgr_tm *tm) {
  int range=hi-lo+1;
  if (range<2) return 0;
  
  //TODO Ought to add special logic for keyboards, they might come in thru the generic bus.
  
  /* 2025-05-16: Hacking in some static maps for provisioning contop.
   */
  if ((tm->vid==0x0e8f)&&(tm->pid==0x0003)) switch (btnid) { // El Cheapo.
    case 0x00010120: return PBL_BTN_NORTH;
    case 0x00010121: return PBL_BTN_EAST;
    case 0x00010122: return PBL_BTN_SOUTH;
    case 0x00010123: return PBL_BTN_WEST;
    case 0x00010124: return PBL_BTN_L1;
    case 0x00010125: return PBL_BTN_R1;
    case 0x00010129: return PBL_BTN_AUX1;
    case 0x0001012b: return PBL_BTN_AUX3; // rp
  }
  
  // If we got a sensible (hidusage) and it agrees with the range, great.
  switch (hidusage) {
    case 0x00010030:
    case 0x00010032:
    case 0x00010033:
        if (range>=3) return PBL_BTN_LEFT|PBL_BTN_RIGHT; 
        break;
    case 0x00010031:
    case 0x00010034:
    case 0x00010035:
        if (range>=3) return PBL_BTN_UP|PBL_BTN_DOWN;
        break;
    case 0x0001003d: return PBL_BTN_AUX1;
    case 0x0001003e: return PBL_BTN_AUX2;
    case 0x00010090: return PBL_BTN_UP;
    case 0x00010091: return PBL_BTN_DOWN;
    case 0x00010092: return PBL_BTN_RIGHT;
    case 0x00010093: return PBL_BTN_LEFT;
  }
  
  // Range of 8, assume it's a hat.
  if (range==8) return PBL_BTN_LEFT|PBL_BTN_RIGHT|PBL_BTN_UP|PBL_BTN_DOWN;
  
  // Range of at least 3, with the resting value in between, assign to the least-used axis.
  if ((range>=3)&&(rest>lo)&&(rest<hi)) return pblrt_inmgr_tm_least_popular_axis(tm);
  
  // Everything else, assign it willy-nilly to thumbs, triggers, or auxes.
  return pblrt_inmgr_tm_least_popular_bit(tm,
    PBL_BTN_SOUTH|PBL_BTN_WEST|PBL_BTN_EAST|PBL_BTN_NORTH|
    PBL_BTN_L1|PBL_BTN_R1|PBL_BTN_L2|PBL_BTN_R2|
    PBL_BTN_AUX1|PBL_BTN_AUX2|PBL_BTN_AUX3
  );
}

/* Validate proposed button mapping.
 * ie, does this btnid make sense for this input range?
 */
 
static int pblrt_button_map_valid(int btnid,int lo,int hi,int rest) {
  if (!btnid) return 0;
  int range=hi-lo+1;
  if (range<2) return 0;
  switch (btnid) {
  
    // Mapping directly to a single button is ok for sure.
    // Note that this does not include PBL_BTN_CD; that's not something you map to.
    case PBL_BTN_LEFT: case PBL_BTN_RIGHT: case PBL_BTN_UP: case PBL_BTN_DOWN:
    case PBL_BTN_SOUTH: case PBL_BTN_WEST: case PBL_BTN_EAST: case PBL_BTN_NORTH:
    case PBL_BTN_L1: case PBL_BTN_R1: case PBL_BTN_L2: case PBL_BTN_R2:
    case PBL_BTN_AUX1: case PBL_BTN_AUX2: case PBL_BTN_AUX3:
      return 1;
    
    // Axis pairs are ok if the range is at least 3.
    case PBL_BTN_LEFT|PBL_BTN_RIGHT:
    case PBL_BTN_UP|PBL_BTN_DOWN:
      return (range>=3);
      
    // The full dpad is ok if the range is exactly 8, for hats.
    case PBL_BTN_LEFT|PBL_BTN_RIGHT|PBL_BTN_UP|PBL_BTN_DOWN:
      return (range==8);
      
    // Anything above the 16-bit range is ok, for stateless signals.
    default: return (btnid>0xffff);
  }
  return 0;
}
  
/* Synthesize template, receive one capability.
 */
 
static int pblrt_inmgr_tm_synthesize_cb(int btnid,int pblbtnid,int hidusage,int lo,int hi,int rest,void *userdata) {
  struct pblrt_inmgr_tm *tm=userdata;
  if (!pblbtnid) {
    if (!(pblbtnid=pblrt_inmgr_guess_btnid(btnid,hidusage,lo,hi,rest,tm))) return 0;
  }
  if (!pblrt_button_map_valid(pblbtnid,lo,hi,rest)) return 0;
  struct pblrt_inmgr_tm_button *button=pblrt_inmgr_tm_add_button(tm,btnid,pblbtnid);
  if (!button) return -1;
  return 0;
}
  
/* Synthesize template, in context.
 */
 
static int pblrt_inmgr_tm_synthesize_internal(struct pblrt_inmgr *inmgr,struct pblrt_inmgr_tm *tm,int devid) {
  int err=pblrt.input->type->list_buttons(pblrt.input,devid,pblrt_inmgr_tm_synthesize_cb,tm);
  if (err<0) return err;
  //TODO Final analysis?
  return 0;
}

/* Synthesize template for device.
 */
 
struct pblrt_inmgr_tm *pblrt_inmgr_tmv_synthesize(struct pblrt_inmgr *inmgr,int vid,int pid,int version,const char *name,int devid) {
  if (!pblrt.input) return 0;
  if (!pblrt.input->type->list_buttons) return 0;
  struct pblrt_inmgr_tm *tm=pblrt_inmgr_tm_new(inmgr,vid,pid,version,name);
  if (!tm) return 0;
  int err=pblrt_inmgr_tm_synthesize_internal(inmgr,tm,devid);
  if (err<0) {
    pblrt_inmgr_tm_del(inmgr,tm);
    return 0;
  }
  return tm;
}

/* Context for applying template.
 */
 
struct pblrt_inmgr_tmapply {
  struct pblrt_inmgr *inmgr;
  struct pblrt_inmgr_device *device;
  const struct pblrt_inmgr_tm *tm;
};

/* Add map to device, with provided template.
 */
 
static int pblrt_inmgr_device_apply_tm_cb(int btnid,int pblbtnid,int hidusage,int lo,int hi,int rest,void *userdata) {
  struct pblrt_inmgr_tmapply *ctx=userdata;
  int p=pblrt_inmgr_tm_buttonv_search(ctx->tm,btnid);
  if (p<0) return 0;
  int dstbtnid=ctx->tm->buttonv[p].dstbtnid;
  struct pblrt_inmgr_device_button *button=pblrt_inmgr_device_buttonv_add(ctx->device,btnid);
  if (!button) return -1;
  button->dstbtnid=dstbtnid;
  
  // Hats are special.
  if (dstbtnid==(PBL_BTN_LEFT|PBL_BTN_RIGHT|PBL_BTN_UP|PBL_BTN_DOWN)) {
    button->mode=PBLRT_BTNMODE_HAT;
    button->srclo=lo;
    button->srchi=hi;
    button->srcvalue=button->dstvalue=lo-1;
    return 0;
  }
  
  // Mapping to either combined axis, it uses THREEWAY and we make up thresholds here.
  if (
    (dstbtnid==(PBL_BTN_LEFT|PBL_BTN_RIGHT))||
    (dstbtnid==(PBL_BTN_UP|PBL_BTN_DOWN))
  ) {
    button->mode=PBLRT_BTNMODE_THREEWAY;
    int mid=(lo+hi)>>1;
    int midlo=(lo+mid)>>1;
    int midhi=(hi+mid)>>1;
    if (midlo>=mid) midlo=mid-1;
    if (midhi<=mid) midhi=mid+1;
    button->srclo=midlo;
    button->srchi=midhi;
    return 0;
  }
  
  // Everything else is two-state, with the threshold at its lowest possible value.
  button->mode=PBLRT_BTNMODE_TWOSTATE;
  button->srclo=lo+1;
  button->srchi=INT_MAX;
  return 0;
}

/* Apply template to device.
 */
 
int pblrt_inmgr_device_apply_tm(struct pblrt_inmgr *inmgr,struct pblrt_inmgr_device *device,const struct pblrt_inmgr_tm *tm) {
  if (!pblrt.input||!pblrt.input->type->list_buttons) return -1;
  struct pblrt_inmgr_tmapply ctx={.inmgr=inmgr,.device=device,.tm=tm};
  return pblrt.input->type->list_buttons(pblrt.input,device->devid,pblrt_inmgr_device_apply_tm_cb,&ctx);
}
