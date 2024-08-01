/* gfx.h
 * Helpers for rendering to Pebble's RGBA framebuffer.
 * Some things to know:
 *  - The framebuffer is always bytewise RGBA with no row padding.
 *  - Since we run in WebAssembly, always little-endian, pixels as u32 are ABGR, alpha in the most-significant bits.
 *  - It's possible to build Pebble games natively for big-endian hosts and we *DO NOT* accomodate that. You'd have to make some changes here.
 *  - Alpha channel in the final framebuffer must always be set to 0xff. I'd prefer to have the platform simply ignore alpha, but that's been a challenge.
 *  - We'll use integer "texid" for global framebuffers. Texid Zero is the main output, others you can load willy-nilly.
 *  - No blending, and when blitting, transparent pixels must be straight zero (even the chroma channels).
 *  - We do use globals and stdlib. We do not use any Pebble APIs.
 */
 
#ifndef GFX_H
#define GFX_H

// It's safe to change this if you need a different limit.
// Beware, we assume that (GFX_TEXTURE_LIMIT**2*4) can't exceed INT_MAX. (which puts the hard limit at 23170).
#define GFX_TEXTURE_SIZE_LIMIT 1024

// Completely arbitrary, just a safety check against runaway allocation.
#define GFX_TEXTURE_COUNT_LIMIT 32

/* The 8 axiswise transforms for blitting.
 * For reference, let's say your sprite's head is up, and his nose right, in the natural orientation.
 *  - 0: Natural.
 *  - XREV: Flip horizontally.
 *  - YREV: Flip vertically.
 *  - XREV|YREV: Rotate half-turn.
 *  - SWAP: Head left, nose down. (unusual).
 *  - SWAP|XREV: Head left, nose up. ie rotate counter-clockwise.
 *  - SWAP|YREV: Head right, nose down. ie rotate clockwise.
 *  - SWAP|XREV|YREV: Head right, nose up. (unusual).
 */
#define GFX_XFORM_XREV 1
#define GFX_XFORM_YREV 2
#define GFX_XFORM_SWAP 4

void gfx_quit();

/* Allocate the framebuffer, and any other global state.
 */
int gfx_init(int fbw,int fbh);

/* Textures can be used as render targets and sources for blitting.
 * Texture zero is the main output. You can't delete it.
 */
void gfx_texture_del(int texid);
int gfx_texture_new(int w,int h);
int gfx_texture_new_rgba(int w,int h,int stride,const void *src,int srcc);

/* Return the global framebuffer.
 * There's no deferral of render ops, so it really is just "return the framebuffer".
 */
void *gfx_finish();

/* Overwrite the whole framebuffer.
 * If this is texture zero, we force it opaque.
 */
void gfx_clear(int dsttexid,int abgr);

/* Geometric primitives.
 * Tracing always means a 1-pixel thick line.
 * In general, trace should only visit pixels that would also be visited by fill.
 * (I'm not sure we hit that 100%).
 */
void gfx_trace_line(int dsttexid,int ax,int ay,int bx,int by,int xbgr);
void gfx_trace_rect(int dsttexid,int x,int y,int w,int h,int xbgr);
void gfx_fill_rect(int dsttexid,int x,int y,int w,int h,int xbgr);
void gfx_trace_trig(int dsttexid,int ax,int ay,int bx,int by,int cx,int cy,int xbgr);
void gfx_fill_trig(int dsttexid,int ax,int ay,int bx,int by,int cx,int cy,int xbgr);
void gfx_trace_oval(int dsttexid,int x,int y,int w,int h,int xbgr);
void gfx_fill_oval(int dsttexid,int x,int y,int w,int h,int xbgr);

/* Copy from one texture to another, with zeroes transparent.
 * Copying from a texture to itself is not allowed.
 * (w,h)<0 to extend to the right or bottom of src texture.
 * Beyond that, (srcx,srcy,w,h) must be in bounds. We reject the whole operation if not.
 * (dstx,dsty,w,h) may exceed output bounds and we clip as you'd expect.
 */
void gfx_blit(
  int dsttexid,int srctexid,
  int dstx,int dsty,
  int srcx,int srcy,
  int w,int h,
  int xform
);

/* Blit from a 1-bit source, using the provided color for nonzero pixels.
 * 1-bit sources have big-endian pixels, ie 0x80 is first, and rows padded to at least 1 byte.
 */
void gfx_blit_onebit(
  int dsttexid,int dstx,int dsty,
  const void *src,int srcstride,int srcw,int srch,
  int srcx,int srcy,
  int w,int h,
  int xform,
  int xbgr
);

#endif
