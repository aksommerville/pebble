#include "image.h"
#include "stdlib/egg-stdlib.h"
#include <stdint.h>

struct rlead_pixel_writer {
  uint8_t *dstrow;
  uint8_t *dstp;
  uint8_t dstmask;
  int remx; // pixels remaining in row
  int remy; // rows remaining in image, including the one in progress
  int w;
  int stride;
};

static void rlead_pixel_writer_init(struct rlead_pixel_writer *writer,uint8_t *dst,int w,int h,int stride) {
  writer->dstrow=dst;
  writer->dstp=dst;
  writer->dstmask=0x80;
  writer->remx=w;
  writer->remy=h;
  writer->w=w;
  writer->stride=stride;
}

static inline void rlead_pixel_write(struct rlead_pixel_writer *writer,int v) {
  if (writer->remx<1) {
    writer->remy--;
    if (writer->remy<1) { writer->remx=writer->remy=0; return; }
    writer->remx=writer->w;
    writer->dstrow+=writer->stride;
    writer->dstp=writer->dstrow;
    writer->dstmask=0x80;
  }
  if (v) (*writer->dstp)|=writer->dstmask;
  // Don't bother clearing bits on zeros -- the output buffer starts zeroed.
  writer->remx--;
  if (writer->dstmask==0x01) {
    writer->dstmask=0x80;
    writer->dstp++;
  } else {
    writer->dstmask>>=1;
  }
}

struct rlead_word_reader {
  const uint8_t *src;
  int srcc; // Bytes remaining in (src), including the current one.
  uint8_t srcmask;
};

static void rlead_word_reader_init(struct rlead_word_reader *reader,const uint8_t *src,int srcc) {
  reader->src=src;
  reader->srcc=srcc;
  reader->srcmask=0x80;
}

static inline int rlead_word_read(struct rlead_word_reader *reader,int len) {
  if (len<1) return 0;
  if (len>30) return 0;
  if (reader->srcc<1) return 0;
  uint32_t word=0;
  while (len-->0) {
    word<<=1;
    if ((*reader->src)&reader->srcmask) word|=1;
    if (reader->srcmask==0x01) {
      reader->srcmask=0x80;
      reader->src++;
      reader->srcc--;
      if (reader->srcc<1) {
        word<<=len;
        break;
      }
    } else {
      reader->srcmask>>=1;
    }
  }
  return word;
}

static void rlead_unfilter(uint8_t *rp,int stride,int h) {
  // Read forward.
  uint8_t *wp=rp+stride;
  int bc=stride*(h-1);
  for (;bc-->0;wp++,rp++) (*wp)^=(*rp);
}

struct image *rlead_decode(const uint8_t *src,int srcc) {
  if (srcc<9) return 0;
  if (memcmp(src,"\x00rld",4)) return 0;
  int w=(src[4]<<8)|src[5];
  int h=(src[6]<<8)|src[7];
  struct image *image=image_new_alloc(1,w,h);
  if (!image) return 0;
  uint8_t flags=src[8];
  if (flags&4) image->hint|=IMAGE_HINT_ALPHA;
  else image->hint|=IMAGE_HINT_LUMA;
  int srcp=9;
  struct rlead_pixel_writer writer;
  rlead_pixel_writer_init(&writer,image->v,w,h,image->stride);
  struct rlead_word_reader reader;
  rlead_word_reader_init(&reader,src+srcp,srcc-srcp);
  int color=(flags&0x02)?1:0;
  
  while (reader.srcc) {
    int word=rlead_word_read(&reader,3);
    int i=word+1;
    while (i-->0) rlead_pixel_write(&writer,color);
    if (word==7) {
      int wordsize=4;
      for (;;) {
        word=rlead_word_read(&reader,wordsize);
        for (i=word;i-->0;) rlead_pixel_write(&writer,color);
        if (word!=(1<<wordsize)-1) break;
        if (wordsize++>=30) {
          image_del(image);
          return 0;
        }
      }
    }
    color^=1;
  }
  
  if (flags&0x01) rlead_unfilter(image->v,image->stride,h);
  return image;
}
