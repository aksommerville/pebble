#include "image.h"
#include "zlib/zlib.h"
#include "stdlib/egg-stdlib.h"
#include <stdint.h>

/* The Paeth predictor.
 */
 
static uint8_t paeth(uint8_t a,uint8_t b,uint8_t c) {
  int p=a+b-c;
  int pa=p-a; if (pa<0) pa=-pa;
  int pb=p-b; if (pb<0) pb=-pb;
  int pc=p-c; if (pc<0) pc=-pc;
  if ((pa<=pb)&&(pa<=pc)) return a;
  if (pb<=pc) return b;
  return c;
}

/* Decode context.
 */
 
struct png_decoder {
  const uint8_t *src;
  int srcc;
  struct image *image; // Yoink and clear if you want to keep it. Presence means IHDR was processed.
  int y; // Row that we're waiting for. (>=image->h) means we're finished.
  int xstride; // Bytes column-to-column for filter purposes, minimum 1.
  uint8_t *rowbuf; // 1+image->stride
  z_stream z;
  int zinit;
};

static void png_decoder_cleanup(struct png_decoder *decoder) {
  image_del(decoder->image);
  if (decoder->rowbuf) free(decoder->rowbuf);
  if (decoder->zinit) inflateEnd(&decoder->z);
}

/* Receive IHDR.
 */
 
static int decode_IHDR(struct png_decoder *decoder,const uint8_t *src,int srcc) {
  if (decoder->image) return -1; // Already got one. Two is an error.
  if (srcc<13) return -1;
  int w=(src[0]<<24)|(src[1]<<16)|(src[2]<<8)|src[3];
  int h=(src[4]<<24)|(src[5]<<16)|(src[6]<<8)|src[7];
  if ((w<1)||(w>0x7fff)) return -1; // Enforce struct image's stricter size limits early.
  if ((h<1)||(h>0x7fff)) return -1;
  uint8_t depth=src[8];
  uint8_t colortype=src[9];
  uint8_t compression=src[10];
  uint8_t filter=src[11];
  uint8_t interlace=src[12];
  if (compression||filter||interlace) {
    // (interlace==1) is legal per spec, but I'm not going to implement it. And the other two can only be zero.
    return -2;
  }
  int chanc;
  switch (colortype) {
    case 0: chanc=1; break;
    case 2: chanc=3; break;
    case 3: chanc=1; break;
    case 4: chanc=2; break;
    case 6: chanc=4; break;
    default: return -1;
  }
  int pixelsize=chanc*depth;
  if (pixelsize<8) {
    if (pixelsize<1) return -1;
    if (8%pixelsize) return -1;
  } else {
    if (pixelsize>64) return -1;
    if (pixelsize&7) return -1;
  }
  decoder->xstride=pixelsize>>3;
  if (decoder->xstride<1) decoder->xstride=1;
  if (!(decoder->image=image_new_alloc(pixelsize,w,h))) return -1;
  if (!(decoder->rowbuf=malloc(1+decoder->image->stride))) return -1;
  if (inflateInit(&decoder->z)<0) return -1;
  decoder->zinit=1;
  decoder->z.next_out=(Bytef*)decoder->rowbuf;
  decoder->z.avail_out=1+decoder->image->stride;
  return 0;
}

/* Unfilter, by algorithm.
 * (prv) may be null.
 */
 
static void png_unfilter_SUB(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int stride,int xstride) {
  memcpy(dst,src,xstride);
  const uint8_t *left=dst;
  dst+=xstride;
  src+=xstride;
  int i=stride-xstride;
  for (;i-->0;dst++,src++,left++) *dst=(*src)+(*left);
}

static void png_unfilter_UP(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int stride,int xstride) {
  if (!prv) {
    memcpy(dst,src,stride);
  } else {
    for (;stride-->0;dst++,src++,prv++) *dst=(*src)+(*prv);
  }
}

static void png_unfilter_AVG(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int stride,int xstride) {
  if (!prv) {
    // what an odd choice of filter...
    memcpy(dst,src,xstride);
    const uint8_t *left=dst;
    dst+=xstride;
    src+=xstride;
    int i=stride-xstride;
    for (;i-->0;dst++,src++,left++) *dst=(*src)+((*left)>>1);
  } else {
    const uint8_t *left=dst;
    int i=xstride;
    for (;i-->0;dst++,src++,prv++) *dst=(*src)+((*prv)>>1);
    for (i=stride-xstride;i-->0;dst++,src++,prv++,left++) *dst=(*src)+(((*left)+(*prv))>>1);
  }
}

static void png_unfilter_PAETH(uint8_t *dst,const uint8_t *src,const uint8_t *prv,int stride,int xstride) {
  if (!prv) {
    png_unfilter_SUB(dst,src,prv,stride,xstride);
  } else {
    const uint8_t *left=dst;
    int i=xstride;
    for (;i-->0;dst++,src++,prv++) *dst=(*src)+(*prv);
    for (i=stride-xstride;i-->0;dst++,src++,prv++,left++) *dst=(*src)+paeth(*left,*prv,prv[-xstride]);
  }
}

/* Receive filtered pixels from (decoder->rowbuf).
 */
 
static int png_decoder_finish_row(struct png_decoder *decoder) {
  if (decoder->y<decoder->image->h) {
    uint8_t *dst=decoder->image->v;
    dst+=decoder->image->stride*decoder->y;
    const uint8_t *prv=0;
    if (decoder->y) prv=dst-decoder->image->stride;
    switch (decoder->rowbuf[0]) {
      case 0: memcpy(dst,decoder->rowbuf+1,decoder->image->stride); break;
      case 1: png_unfilter_SUB(dst,decoder->rowbuf+1,prv,decoder->image->stride,decoder->xstride); break;
      case 2: png_unfilter_UP(dst,decoder->rowbuf+1,prv,decoder->image->stride,decoder->xstride); break;
      case 3: png_unfilter_AVG(dst,decoder->rowbuf+1,prv,decoder->image->stride,decoder->xstride); break;
      case 4: png_unfilter_PAETH(dst,decoder->rowbuf+1,prv,decoder->image->stride,decoder->xstride); break;
      default: return -1;
    }
    decoder->y++;
  }
  return 0;
}

/* Receive IDAT.
 */
 
static int decode_IDAT(struct png_decoder *decoder,const uint8_t *src,int srcc) {
  if (!decoder->zinit) return -1; // IDAT before IHDR, perhaps?
  decoder->z.next_in=(Bytef*)src;
  decoder->z.avail_in=srcc;
  while (decoder->z.avail_in) {
  
    if (!decoder->z.avail_out) {
      if (png_decoder_finish_row(decoder)<0) return -1;
      decoder->z.next_out=(Bytef*)decoder->rowbuf;
      decoder->z.avail_out=1+decoder->image->stride;
    }
    
    int err=inflate(&decoder->z,Z_NO_FLUSH);
    if (err<0) return -1;
  }
  return 0;
}

/* Finalize decode. Verify we got all the things.
 */
 
static int decode_finish(struct png_decoder *decoder) {
  if (!decoder->image) return -1;
  
  decoder->z.next_in=0;
  decoder->z.avail_in=0;
  for (;;) {
    if (!decoder->z.avail_out) {
      if (png_decoder_finish_row(decoder)<0) return -1;
      decoder->z.next_out=(Bytef*)decoder->rowbuf;
      decoder->z.avail_out=1+decoder->image->stride;
    }
    int err=inflate(&decoder->z,Z_FINISH);
    if (err<0) return -1;
    if (err==Z_STREAM_END) {
      if (!decoder->z.avail_out) {
        png_decoder_finish_row(decoder);
      }
      break;
    }
  }
  
  if (decoder->y<decoder->image->h) return -1;
  return 0;
}

/* Decode in context.
 * Caller validates signature, we do all the rest.
 */
 
static int image_decode_png_inner(struct png_decoder *decoder) {
  int srcp=8;
  int stopp=decoder->srcc-8;
  while (srcp<=stopp) {
    int chunklen=decoder->src[srcp++]<<24;
    chunklen|=decoder->src[srcp++]<<16;
    chunklen|=decoder->src[srcp++]<<8;
    chunklen|=decoder->src[srcp++];
    const uint8_t *chunkid=decoder->src+srcp;
    srcp+=4;
    const uint8_t *payload=decoder->src+srcp;
    srcp+=chunklen;
    srcp+=4; // Don't bother validating CRC.
    if (srcp>decoder->srcc) return -1;
    int err=0;
    if (!memcmp(chunkid,"IHDR",4)) err=decode_IHDR(decoder,payload,chunklen);
    else if (!memcmp(chunkid,"IDAT",4)) err=decode_IDAT(decoder,payload,chunklen);
    if (err<0) return err;
  }
  return decode_finish(decoder);
}

/* Zlib malloc wrappers, necessary because we compile with Z_SOLO.
 */

static voidpf zalloc(voidpf userdata,uInt items,uInt size) {
  return calloc(items,size);
}

static void zfree(voidpf userdata,voidpf address) {
  free(address);
}

/* Decode PNG, main entry point.
 */
  
struct image *png_decode(const uint8_t *src,int srcc) {
  if ((srcc<8)||memcmp(src,"\x89PNG\r\n\x1a\n",8)) return 0;
  struct png_decoder decoder={
    .src=src,
    .srcc=srcc,
    .z={
      .zalloc=zalloc,
      .zfree=zfree,
    },
  };
  int err=image_decode_png_inner(&decoder);
  if (err<0) {
    png_decoder_cleanup(&decoder);
    return 0;
  }
  struct image *image=decoder.image;
  decoder.image=0;
  png_decoder_cleanup(&decoder);
  return image;
}
