#include "rom.h"

/* Iterate ROM file.
 */
 
int rom_for_each(
  const void *rom,int romc,
  int (*cb)(int tid,int rid,const void *serial,int serialc,void *userdata),
  void *userdata
) {
  if (romc<8) return -1;
  const unsigned char *src=rom;
  if (src[0]!=0x00) return -1;
  if (src[1]!=0xff) return -1;
  if (src[2]!='P') return -1;
  if (src[3]!='B') return -1;
  if (src[4]!='L') return -1;
  if (src[5]!='R') return -1;
  if (src[6]!='O') return -1;
  if (src[7]!='M') return -1;
  int srcp=8,tid=1,rid=1,err;
  while (srcp<romc) {
    unsigned char lead=src[srcp++];
    if (!lead) break;
    switch (lead&0xc0) {
      case 0x00: {
          tid+=lead;
          if (tid>0x3f) return -1;
          rid=1;
        } break;
      case 0x40: {
          if (srcp>romc-1) return -1;
          int d=(lead&0x3f)<<8;
          d|=src[srcp++];
          rid+=d;
          if (rid>0xffff) return -1;
        } break;
      case 0x80: {
          int l=(lead&0x3f);
          l+=1;
          if (srcp>romc-l) return -1;
          if (rid>0xffff) return -1;
          if (err=cb(tid,rid,src+srcp,l,userdata)) return err;
          srcp+=l;
          rid++;
        } break;
      case 0xc0: {
          if (srcp>romc-2) return -1;
          int l=(lead&0x3f)<<16;
          l|=src[srcp++]<<8;
          l|=src[srcp++];
          l+=65;
          if (srcp>romc-l) return -1;
          if (rid>0xffff) return -1;
          if (err=cb(tid,rid,src+srcp,l,userdata)) return err;
          srcp+=l;
          rid++;
        } break;
    }
  }
  return 0;
}

/* Get one resource.
 */
 
struct rom_seek_ctx {
  int tid,rid;
  void *dstpp;
};

static int rom_seek_cb(int tid,int rid,const void *serial,int serialc,void *userdata) {
  struct rom_seek_ctx *ctx=userdata;
  if (tid>ctx->tid) return -1;
  if (tid<ctx->tid) return 0;
  if (rid>ctx->rid) return -1;
  if (rid<ctx->rid) return 0;
  *(const void**)(ctx->dstpp)=serial;
  return serialc;
}

int rom_seek(void *dstpp,const void *rom,int romc,int tid,int rid) {
  struct rom_seek_ctx ctx={.dstpp=dstpp,.tid=tid,.rid=rid};
  int dstc=rom_for_each(rom,romc,rom_seek_cb,&ctx);
  if (dstc<0) return 0;
  return dstc;
}

/* Iterate metadata.
 */
 
int rom_metadata_for_each(
  const void *meta,int srcc,
  int (*cb)(const char *k,int kc,const char *v,int vc,void *userdata),
  void *userdata
) {
  if (!meta||(srcc<4)) return -1;
  const unsigned char *src=meta;
  if (src[0]!=0x00) return -1;
  if (src[1]!='P') return -1;
  if (src[2]!='M') return -1;
  if (src[3]!=0xff) return -1;
  int srcp=4,err;
  for (;;) {
    if (srcp>srcc-2) return 0;
    int kc=src[srcp++];
    if (!kc) return 0;
    int vc=src[srcp++];
    if (srcp>srcc-vc-kc) return -1;
    const char *k=(char*)src+srcp; srcp+=kc;
    const char *v=(char*)src+srcp; srcp+=vc;
    if (err=cb(k,kc,v,vc,userdata)) return err;
  }
}

/* Iterate strings.
 */

int rom_strings_for_each(
  const void *strings,int srcc,
  int (*cb)(int index,const char *v,int c,void *userdata),
  void *userdata
) {
  if (!strings||(srcc<4)) return -1;
  const unsigned char *src=strings;
  if (src[0]!=0x00) return -1;
  if (src[1]!='P') return -1;
  if (src[2]!='S') return -1;
  if (src[3]!=0xff) return -1;
  int srcp=4,index=0,err;
  for (;;index++) {
    if (srcp>srcc-2) return 0;
    int len=src[srcp++]<<8;
    len|=src[srcp++];
    if (srcp>srcc-len) return -1;
    if (!len) continue;
    if (err=cb(index,(char*)src+srcp,len,userdata)) return err;
    srcp+=len;
  }
}

/* Fetch one string.
 */
 
int rom_strings_get_by_index(void *dstpp,const void *strings,int srcc,int index) {
  if (!strings||(srcc<4)) return 0;
  if (index<0) return 0;
  const unsigned char *src=strings;
  if (src[0]!=0x00) return 0;
  if (src[1]!='P') return 0;
  if (src[2]!='S') return 0;
  if (src[3]!=0xff) return 0;
  int srcp=4,err;
  for (;;) {
    if (srcp>srcc-2) return 0;
    int len=src[srcp++]<<8;
    len|=src[srcp++];
    if (srcp>srcc-len) return 0;
    if (!index--) {
      *(const void**)dstpp=src+srcp;
      return len;
    }
    srcp+=len;
  }
}
