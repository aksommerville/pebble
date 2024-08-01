#ifndef GFX_INTERNAL_H
#define GFX_INTERNAL_H

#include "gfx.h"
#include "stdlib/egg-stdlib.h"
#include <stdint.h>

struct gfx_texture {
  int *v;
  int w,h; // stride is always (w) ints.
};

extern struct gfx {
  // (texturev) is indexed by (texid). It may be sparse.
  struct gfx_texture *texturev;
  int texturec,texturea;
} gfx;

/* Texture iterator.
 * These are stupid simple, owing to our 32-bit-only requirement.
 **************************************************************/
 
struct gfx_iter {
  int *p;
  int *pmajor;
  int cminor,cmajor; // How many remaining *after* current
  int dminor,dmajor;
  int cminor0;
};

/* Rect must be nonzero and in bounds or we fail.
 * (xform) only refers to the origin and direction of travel. Swapping (w,h) is up to you if you want to.
 * After a success, we are pointed to a valid pixel.
 */
int gfx_iter_init(struct gfx_iter *iter,const struct gfx_texture *texture,int x,int y,int w,int h,uint8_t xform);

/* Advance and return nonzero if we're pointed to a valid pixel.
 * Zero if finished.
 */
static inline int gfx_iter_next(struct gfx_iter *iter) {
  if (iter->cminor--) {
    iter->p+=iter->dminor;
    return 1;
  }
  if (iter->cmajor--) {
    iter->pmajor+=iter->dmajor;
    iter->p=iter->pmajor;
    iter->cminor=iter->cminor0;
    return 1;
  }
  return iter->cminor=iter->cmajor=0;
}

/* gfx_safeiter is a variation on gfx_iter which allows OOB positions.
 * You can keep nexting and it will travel the full requested bounds.
 * Before accessing a pixel, you must check gfx_safeiter_in_bounds().
 */
struct gfx_safeiter {
  struct gfx_iter hdr;
  int x,y;
  int r,b;
  int dxminor,dyminor;
  int dxmajor,dymajor;
  int xmajor,ymajor;
};

int gfx_safeiter_init(struct gfx_safeiter *iter,const struct gfx_texture *texture,int x,int y,int w,int h,uint8_t xform);

static inline int gfx_safeiter_in_bounds(const struct gfx_safeiter *iter) {
  return ((iter->x>=0)&&(iter->y>=0)&&(iter->x<iter->r)&&(iter->y<iter->b));
}

int gfx_safeiter_next(struct gfx_safeiter *iter);

/* Line walker.
 **************************************************/
 
struct gfx_linewalker {
  int x,y;
  int dx,dy;
  int weight;
  int xweight,yweight;
  int xthresh,ythresh;
  int bx,by;
  int ax,ay;
  int *p;
  int stride;
};

/* After initialization, (lw) is pointing to (ax,ay).
 * (texture) is optional. If you provide it, we'll keep (lw->p) pointing to the current pixel.
 */
void gfx_linewalker_init(struct gfx_linewalker *lw,int ax,int ay,int bx,int by,struct gfx_texture *texture);

/* Step to an adjacent pixel, cardinal or diagonal.
 * Or step until we reach the next row, and advance along that row as far as we can.
 * We check for point B and will not move beyond it.
 * (to_row_end) is useful at initialization if you're going to use (step_y); it advances to the end of the current row.
 * That happens automatically after step_y, so you only need it for the first row.
 */
void gfx_linewalker_step(struct gfx_linewalker *lw);
void gfx_linewalker_step_y(struct gfx_linewalker *lw);
void gfx_linewalker_to_row_end(struct gfx_linewalker *lw);

uint8_t gfx_linewalker_progress_u08(const struct gfx_linewalker *lw);

#endif
