/* image.h
 * Client-side image decoder for Pebble.
 * Requires libc.
 */
 
#ifndef IMAGE_H
#define IMAGE_H

/* Delete these symbols, or set to zero, to eliminate a format.
 * Each of these has a corresponding C file.
 * OK to delete the files for disabled formats.
 * Each format must export:
 *   struct image *FORMAT_decode(const void *src,int srcc);
 * Format detection is hard-coded in image.c; add yourself there if you add a format.
 */
#define IMAGE_FORMAT_rawimg 1
#define IMAGE_FORMAT_qoi    2
#define IMAGE_FORMAT_rlead  3
#define IMAGE_FORMAT_png    4 /* Also requires zlib. */

#define IMAGE_HINT_ALPHA 0x01
#define IMAGE_HINT_LUMA  0x02

struct image {
  void *v;
  int w,h;
  int stride; // Distance row-to-row in bytes.
  
  /* Format is determined entirely by (pixelsize).
   * Decoders must convert from whatever their format provides, to one of our standard options.
   *  - 1: Luma or alpha. Big-endian. Rows pad to one byte minimum.
   *  - 2: Index (ie undefined). Big-endian. Rows pad to one byte minimum.
   *  - 4: Index (ie undefined). Big-endian. Rows pad to one byte minimum.
   *  - 8: Luma or alpha. Big-endian. Rows pad to one byte minimum.
   *  - 16: RGBA5551 big-endian.
   *  - 24: RGB888.
   *  - 32: RGBA8888.
   */
  int pixelsize;
  
  int hint;
};

void image_del(struct image *image);

struct image *image_decode(const void *src,int srcc);

struct image *image_new_alloc(int pixelsize,int w,int h);

/* Generically rewrite an image in place.
 * (pixelsize,w,h) zero for no change.
 * (cvt) zero for default conversion based on pixelsize.
 * We force stride to its minimum.
 * May replace (image->v) on success, not guaranteed to.
 */
int image_reformat_in_place(
  struct image *image,
  int pixelsize,int w,int h,
  int (*cvt)(int pixel,void *userdata),
  void *userdata
);

/* Conveniences around image_reformat_in_place().
 * "canonicalize" means either 32 or 1 bit.
 * Normally, image_force_rgba is what you want. The image is then ready for upload to gfx.
 */
int image_force_rgba(struct image *image);
int image_canonicalize(struct image *image);

static inline int image_get_pixels_length(const struct image *image) {
  return image->stride*image->h;
}

#endif
