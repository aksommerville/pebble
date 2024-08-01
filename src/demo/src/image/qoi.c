#include "image.h"
#include "stdlib/egg-stdlib.h"
#include <stdint.h>

struct image *qoi_decode(const uint8_t *src,int srcc) {
  if (srcc<14) return 0;
  if (memcmp(src,"qoif",4)) return 0;
  int w=(src[4]<<24)|(src[5]<<16)|(src[6]<<8)|src[7];
  int h=(src[8]<<24)|(src[9]<<16)|(src[10]<<8)|src[11];
  struct image *image=image_new_alloc(32,w,h);
  if (!image) return 0;
  #define FAIL { image_del(image); return 0; }
  int srcp=14;
  uint8_t *dst=image->v;
  int dstp=0,dstc=image_get_pixels_length(image);
  uint8_t cache[64*4]={0};
  uint8_t prv[4]={0,0,0,0xff};
  while ((srcp<srcc)&&(dstp<dstc)) {
    uint8_t lead=src[srcp++];
    
    if (lead==0xfe) { // 11111110 rrrrrrrr gggggggg bbbbbbbb : QOI_OP_RGB. Alpha from previous pixel.
      if (srcp>srcc-3) FAIL
      uint8_t r=src[srcp++];
      uint8_t g=src[srcp++];
      uint8_t b=src[srcp++];
      dst[dstp++]=r;
      dst[dstp++]=g;
      dst[dstp++]=b;
      dst[dstp++]=prv[3];
      prv[0]=r;
      prv[1]=g;
      prv[2]=b;
      int cachep=((r*3+g*5+b*7+prv[3]*11)&0x3f)<<2;
      memcpy(cache+cachep,prv,4);
      continue;
    }
    
    if (lead==0xff) { // 11111111 rrrrrrrr gggggggg bbbbbbbb aaaaaaaa : QOI_OP_RGBA
      if (srcp>srcc-4) FAIL
      uint8_t r=src[srcp++];
      uint8_t g=src[srcp++];
      uint8_t b=src[srcp++];
      uint8_t a=src[srcp++];
      dst[dstp++]=r;
      dst[dstp++]=g;
      dst[dstp++]=b;
      dst[dstp++]=a;
      prv[0]=r;
      prv[1]=g;
      prv[2]=b;
      prv[3]=a;
      int cachep=((r*3+g*5+b*7+a*11)&0x3f)<<2;
      memcpy(cache+cachep,prv,4);
      continue;
    }
    
    switch (lead&0xc0) {
      case 0x00: { // 00iiiiii : QOI_OP_INDEX. Emit cache[i].
          int cachep=lead<<2;
          memcpy(dst+dstp,cache+cachep,4);
          memcpy(prv,cache+cachep,4);
          dstp+=4;
        } break;
        
      case 0x40: { // 01rrggbb : QOI_OP_DIFF. (r,g,b) are (-2..1), difference from previous pixel.
          int dr=((lead>>4)&3)-2;
          int dg=((lead>>2)&3)-2;
          int db=(lead&3)-2;
          uint8_t r=prv[0]+dr;
          uint8_t g=prv[1]+dg;
          uint8_t b=prv[2]+db;
          dst[dstp++]=r;
          dst[dstp++]=g;
          dst[dstp++]=b;
          dst[dstp++]=prv[3];
          prv[0]=r;
          prv[1]=g;
          prv[2]=b;
          int cachep=((r*3+g*5+b*7+prv[3]*11)&0x3f)<<2;
          memcpy(cache+cachep,prv,4);
        } break;
        
      case 0x80: { // 10gggggg rrrrbbbb : QOI_OP_LUMA. (g) in (-32..31), (r,b) in (-8..7)+(g), difference from previous pixel.
          if (srcp>srcc-1) FAIL
          int dg=(lead&0x3f)-32;
          uint8_t next=src[srcp++];
          int dr=(next>>4)-8+dg;
          int db=(next&0xf)-8+dg;
          uint8_t r=prv[0]+dr;
          uint8_t g=prv[1]+dg;
          uint8_t b=prv[2]+db;
          dst[dstp++]=r;
          dst[dstp++]=g;
          dst[dstp++]=b;
          dst[dstp++]=prv[3];
          prv[0]=r;
          prv[1]=g;
          prv[2]=b;
          int cachep=((r*3+g*5+b*7+prv[3]*11)&0x3f)<<2;
          memcpy(cache+cachep,prv,4);
        } break;
        
      case 0xc0: { // 11llllll : QOI_OP_RUN. Emit previous pixel (l+1) times, 1..62. 63 and 64 are illegal.
          int len=(lead&0x3f)+1;
          while ((len-->0)&&(dstp<dstc)) {
            memcpy(dst+dstp,prv,4);
            dstp+=4;
          }
        } break;
        
    }
  }
  // Spec implies that incomplete data is an error. We won't enforce that.
  return image;
}
