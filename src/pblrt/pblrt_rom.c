/* pblrt_rom.c
 * Utilities for reading things off the global ROM file.
 */

#include "pblrt_internal.h"

/* Get resource.
 */
 
int pblrt_rom_get(void *dstpp,uint8_t tid,uint16_t rid) {
  //TODO We could dramatically improve search performance by pre-collating a TOC.
  // Consider whether the extra memory, load time, and effort is worth the search improvement.
  // (probly yes...)
  int stid=1,srid=1;
  const uint8_t *src=pblrt.rom;
  int srcc=pblrt.romc,srcp=8;
  while (srcp<srcc) {
    uint8_t lead=src[srcp++];
    if (!lead) break;
    switch (lead&0xc0) {
      case 0x00: {
          stid+=lead;
          if (stid>tid) return 0;
          srid=1;
        } break;
      case 0x40: {
          if (srcp>srcc-1) return 0;
          int d=(lead&0x3f)<<8;
          d|=src[srcp++];
          srid+=d;
        } break;
      case 0x80: {
          int len=(lead&0x3f)+1;
          if (srcp>srcc-len) return 0;
          if ((stid==tid)&&(srid==rid)) {
            *(const void**)dstpp=src+srcp;
            return len;
          }
          srcp+=len;
          srid++;
        } break;
      case 0xc0: {
          if (srcp>srcc-2) return 0;
          int len=(lead&0x3f)<<16;
          len|=src[srcp++]<<8;
          len|=src[srcp++];
          len+=65;
          if (srcp>srcc-len) return 0;
          if ((stid==tid)&&(srid==rid)) {
            *(const void**)dstpp=src+srcp;
            return len;
          }
          srcp+=len;
          srid++;
        } break;
    }
  }
  return 0;
}

/* Get metadata field.
 */
 
int pblrt_rom_get_metadata(void *dstpp,const char *k,int kc) {
  if (!k) return 0;
  if (kc<0) { kc=0; while (k[kc]) kc++; }

  // If we haven't registered the metadata resource yet, find and validate it.
  if (!pblrt.metadatap) {
    const uint8_t *src=0;
    int srcc=pblrt_rom_get(&src,PBL_TID_metadata,1);
    if (srcc<1) {
      fprintf(stderr,"%s: Required resource metadata:1 not found!\n",pblrt.romname);
      return -2;
    }
    if ((srcc<4)||memcmp(src,"\x00PM\xff",4)) {
      fprintf(stderr,"%s: Malformed metadata, signature check failed.\n",pblrt.romname);
      return -2;
    }
    int srcp=4;
    for (;;) {
      if (srcp>=srcc) {
        fprintf(stderr,"%s: Malformed metadata, terminator missing.\n",pblrt.romname);
        return -2;
      }
      uint8_t kc=src[srcp++];
      if (!kc) break;
      uint8_t vc=src[srcp++];
      if (srcp>srcc-vc-kc) {
        fprintf(stderr,"%s: Malformed metadata, content overrun.\n",pblrt.romname);
        return -2;
      }
      srcp+=kc+vc;
    }
    pblrt.metadatap=(src-(uint8_t*)pblrt.rom)+4;
    if (pblrt.metadatap<1) return -1;
  }
  
  // metadata:1 has been located and validated. We can iterate it, confident that overruns are impossible.
  const uint8_t *src=(uint8_t*)pblrt.rom+pblrt.metadatap;
  int srcp=0;
  for (;;) {
    uint8_t qkc=src[srcp++];
    if (!qkc) return 0;
    uint8_t qvc=src[srcp++];
    if ((qkc==kc)&&!memcmp(k,src+srcp,kc)) {
      *(const void**)dstpp=src+srcp+kc;
      return qvc;
    }
    srcp+=qkc+qvc;
  }
}

/* Get metadata field with string resolution.
 */

int pblrt_rom_get_metadata_text(void *dstpp,const char *k,int kc) {
  if (!k) return 0;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (kc<255) { // ==255 is legal, but can't have anything appended to it.
    char altk[255];
    memcpy(altk,k,kc);
    altk[kc]='$';
    const char *v;
    int vc=pblrt_rom_get_metadata(&v,altk,kc+1);
    if (vc>0) {
      int index=0;
      if ((sr_int_eval(&index,v,vc)>=2)&&(index>=0)) {
        if ((vc=pblrt_rom_get_string(dstpp,1,index))>0) return vc;
      }
    }
    altk[kc]='@';
    if ((vc=pblrt_rom_get_metadata(dstpp,altk,kc+1))>0) return vc;
  }
  return pblrt_rom_get_metadata(dstpp,k,kc);
}

/* Get one string from a resource.
 */

int pblrt_rom_get_string(void *dstpp,int rid,int index) {
  if ((rid<1)||(rid>=0x40)||(index<0)) return 0;
  const char *src=0;
  int srcc=pblrt_rom_get(&src,PBL_TID_strings,(pblrt.lang<<6)|rid);
  if ((srcc<4)||memcmp(src,"\x00PS\xff",4)) return 0;
  int srcp=4;
  while (srcp<srcc) {
    int len=src[srcp++]<<8;
    if (srcp>=srcc) return 0;
    len|=src[srcp++];
    if (srcp>srcc-len) return 0;
    if (!index--) {
      *(const void**)dstpp=src+srcp;
      return len;
    }
    srcp+=len;
  }
  return 0;
}
