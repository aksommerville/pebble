#include "rom.h"
#include "stdlib/egg-stdlib.h"
#include "pebble/pebble.h"

/* Globals.
 */
 
static struct {
  int lang;
  void *serial;
  int serialc;
  struct rom_res {
    int id; // (tid<<16)|rid;
    const void *v; // WEAK, points into (rom.serial)
    int c;
  } *resv;
  int resc,resa;
} rom={0};

/* Cleanup.
 */
 
void rom_quit() {
  if (rom.serial) free(rom.serial);
  if (rom.resv) free(rom.resv);
  memset(&rom,0,sizeof(rom));
}

/* Decode TOC initially. Receive single resource.
 * These will always arrive in order, no need to search for insertion point.
 */
 
static int rom_init_cb_res(int tid,int rid,const void *v,int c,void *userdata) {
  if (rom.resc>=rom.resa) {
    int na=rom.resa+256;
    if (na>INT_MAX/sizeof(struct rom_res)) return -1;
    void *nv=realloc(rom.resv,sizeof(struct rom_res)*na);
    if (!nv) return -1;
    rom.resv=nv;
    rom.resa=na;
  }
  if ((tid<1)||(tid>0x3f)) return -1;
  if ((rid<1)||(rid>0xffff)) return -1;
  int id=(tid<<16)|rid;
  // It's safe to assume that we're receiving resources in order and metadata:1 is first.
  // This client app should have failed to launch if either of those is false.
  // But it's also easy to validate, so let's do.
  if (rom.resc) {
    const struct rom_res *res=rom.resv+rom.resc-1;
    if (id<=res->id) return -1;
  } else {
    if (tid!=PBL_TID_metadata) return -1;
    if (rid!=1) return -1;
  }
  // OK add it!
  struct rom_res *res=rom.resv+rom.resc++;
  res->id=id;
  res->v=v;
  res->c=c;
  return 0;
}

/* Init.
 */

int rom_init() {
  if (rom.serialc) return 0;
  if ((rom.serialc=pbl_rom_get(0,0))<8) {
    rom.serialc=0;
    return -1;
  }
  if (!(rom.serial=malloc(rom.serialc))) {
    rom.serialc=0;
    return -1;
  }
  if (pbl_rom_get(rom.serial,rom.serialc)!=rom.serialc) {
    rom_quit();
    return -1;
  }
  rom.lang=pbl_get_global_language();
  if (rom_for_each(rom.serial,rom.serialc,rom_init_cb_res,0)<0) {
    rom_quit();
    return -1;
  }
  return 0;
}

/* Language.
 */

void rom_set_language(int lang) {
  // Force it to 10 bits.
  rom.lang=lang&0x3ff;
}

int rom_get_language() {
  return rom.lang;
}

/* Get resource.
 */

int rom_get(void *dstpp,int tid,int rid) {
  if ((tid<1)||(tid>0x3f)||(rid<1)||(rid>0xffff)) return 0;
  int id=(tid<<16)|rid;
  int lo=0,hi=rom.resc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct rom_res *res=rom.resv+ck;
         if (id<res->id) hi=ck;
    else if (id>res->id) lo=ck+1;
    else {
      *(const void**)dstpp=res->v;
      return res->c;
    }
  }
  return 0;
}

/* Get metadata field with string resolution.
 */
 
struct rom_get_metadata_ctx {
  void *dstpp;
  const char *k;
  int kc;
  const char *fallback;
  int fallbackc;
};

static int rom_get_metadata_cb(const char *k,int kc,const char *v,int vc,void *userdata) {
  struct rom_get_metadata_ctx *ctx=userdata;
  // Exact match, return immediately. Use -1 if empty, outer layer will replace with empty.
  if ((kc==ctx->kc)&&!memcmp(k,ctx->k,kc)) {
    *(const void**)(ctx->dstpp)=v;
    if (!vc) {
      ctx->fallbackc=0;
      return -1;
    }
    return 0;
  }
  // Look for '@' and '$' suffix.
  if ((kc==ctx->kc+1)&&!memcmp(k,ctx->k,ctx->kc)) {
    if (k[ctx->kc]=='@') { // '@' = fallback. Record it but keep looking.
      ctx->fallback=v;
      ctx->fallbackc=vc;
      return 0;
    }
    if (k[ctx->kc]=='$') { // '$' = index in strings:1. Return immediately if not empty.
      if (vc) {
        int index=0;
        for (;vc-->0;v++) {
          if ((*v<'0')||(*v>'9')) { index=-1; break; }
          index*=10;
          index+=*v-'0';
        }
        return rom_get_string(ctx->dstpp,1,index);
      }
    }
  }
  return 0;
}

int rom_get_metadata(void *dstpp,const char *k,int kc) {
  if (!k) return 0;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!kc) return 0;
  // We know that metadata:1 is first in the list; just do a quick confirmation.
  if (rom.resc<1) return 0;
  if (rom.resv->id!=((PBL_TID_metadata<<16)|1)) return 0;
  struct rom_get_metadata_ctx ctx={
    .dstpp=dstpp,
    .k=k,
    .kc=kc,
  };
  int err=rom_metadata_for_each(rom.resv->v,rom.resv->c,rom_get_metadata_cb,&ctx);
  if (err>0) return err;
  if (ctx.fallbackc) {
    *(const void**)dstpp=ctx.fallback;
    return ctx.fallbackc;
  }
  return 0;
}

/* Get string.
 */

int rom_get_string(void *dstpp,int subrid,int index) {
  if ((subrid<1)||(subrid>0x3f)) return 0;
  if (index<0) return 0;
  const void *src=0;
  int srcc=rom_get(&src,PBL_TID_strings,(rom.lang<<6)|subrid);
  return rom_strings_get_by_index(dstpp,src,srcc,index);
}
