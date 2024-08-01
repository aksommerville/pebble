#include "image.h"
#include "stdlib/egg-stdlib.h"
#include <stdint.h>

struct image *rawimg_decode(const uint8_t *src,int srcc) {
  if (srcc<9) return 0;
  if (memcmp(src,"\x00rIm",4)) return 0;
  int w=(src[4]<<8)|src[5];
  int h=(src[6]<<8)|src[7];
  int pixelsize=src[8];
  // Requirements for image_new_alloc() are exactly the same as our own, so we lean on it for validation.
  struct image *image=image_new_alloc(pixelsize,w,h);
  if (!image) return 0;
  int available=srcc-9;
  if (image_get_pixels_length(image)!=available) {
    image_del(image);
    return 0;
  }
  memcpy(image->v,src+9,available);
  return image;
}
