#include "gfx_internal.h"

/* Prepare iterator.
 */
 
int gfx_iter_init(struct gfx_iter *iter,const struct gfx_texture *texture,int x,int y,int w,int h,uint8_t xform) {
  if (!iter||!texture||!texture->v) return -1;
  if ((x<0)||(w<1)||(x>texture->w-w)) return -1;
  if ((y<0)||(h<1)||(y>texture->h-h)) return -1;
  
  // Set up initially for xform zero, ie LRTB.
  iter->pmajor=texture->v+y*texture->w+x;
  iter->cmajor=h-1;
  iter->cminor0=w-1;
  iter->dmajor=texture->w;
  iter->dminor=1;
  
  if (xform&GFX_XFORM_XREV) {
    iter->pmajor+=w-1;
    iter->dminor=-1;
  }
  if (xform&GFX_XFORM_YREV) {
    iter->pmajor+=texture->w*(h-1);
    iter->dmajor=-texture->w;
  }
  if (xform&GFX_XFORM_SWAP) {
    int tmp;
    tmp=iter->dmajor;
    iter->dmajor=iter->dminor;
    iter->dminor=tmp;
    tmp=iter->cminor0;
    iter->cminor0=iter->cmajor;
    iter->cmajor=tmp;
  }
  
  iter->p=iter->pmajor;
  iter->cminor=iter->cminor0;
  return 0;
}

/* Safe iterator.
 */
 
int gfx_safeiter_init(struct gfx_safeiter *iter,const struct gfx_texture *texture,int x,int y,int w,int h,uint8_t xform) {
  if (!iter||!texture||!texture->v) return -1;
  if ((w<1)||(h<1)) return -1;
  
  // Set up initially for xform zero, ie LRTB.
  iter->hdr.pmajor=texture->v+y*texture->w+x;
  iter->hdr.cmajor=h-1;
  iter->hdr.cminor0=w-1;
  iter->hdr.dmajor=texture->w;
  iter->hdr.dminor=1;
  iter->r=texture->w;
  iter->b=texture->h;
  iter->xmajor=x;
  iter->ymajor=y;
  iter->dxmajor=0;
  iter->dymajor=1;
  iter->dxminor=1;
  iter->dyminor=0;
  
  if (xform&GFX_XFORM_XREV) {
    iter->hdr.pmajor+=w-1;
    iter->hdr.dminor=-1;
    iter->xmajor+=w-1;
    iter->dxminor=-1;
  }
  if (xform&GFX_XFORM_YREV) {
    iter->hdr.pmajor+=texture->w*(h-1);
    iter->hdr.dmajor=-texture->w;
    iter->ymajor+=h-1;
    iter->dymajor=-1;
  }
  if (xform&GFX_XFORM_SWAP) {
    int tmp;
    tmp=iter->hdr.dmajor;
    iter->hdr.dmajor=iter->hdr.dminor;
    iter->hdr.dminor=tmp;
    tmp=iter->hdr.cminor0;
    iter->hdr.cminor0=iter->hdr.cmajor;
    iter->hdr.cmajor=tmp;
    tmp=iter->dxmajor;
    iter->dxmajor=iter->dymajor;
    iter->dymajor=tmp;
    tmp=iter->dxminor;
    iter->dxminor=iter->dyminor;
    iter->dyminor=tmp;
  }
  
  iter->hdr.p=iter->hdr.pmajor;
  iter->hdr.cminor=iter->hdr.cminor0;
  iter->x=iter->xmajor;
  iter->y=iter->ymajor;
  return 0;
}

int gfx_safeiter_next(struct gfx_safeiter *iter) {
  if (iter->hdr.cminor--) {
    iter->hdr.p+=iter->hdr.dminor;
    iter->x+=iter->dxminor;
    iter->y+=iter->dyminor;
    return 1;
  }
  if (iter->hdr.cmajor--) {
    iter->hdr.pmajor+=iter->hdr.dmajor;
    iter->hdr.p=iter->hdr.pmajor;
    iter->hdr.cminor=iter->hdr.cminor0;
    iter->xmajor+=iter->dxmajor;
    iter->ymajor+=iter->dymajor;
    iter->x=iter->xmajor;
    iter->y=iter->ymajor;
    return 1;
  }
  return iter->hdr.cminor=iter->hdr.cmajor=0;
}

/* Line walker.
 */
 
void gfx_linewalker_init(struct gfx_linewalker *lw,int ax,int ay,int bx,int by,struct gfx_texture *texture) {
  lw->x=ax;
  lw->y=ay;
  lw->ax=ax;
  lw->ay=ay;
  lw->bx=bx;
  lw->by=by;
  if (ax<bx) {
    lw->xweight=bx-ax;
    lw->dx=1;
  } else if (ax>bx) {
    lw->xweight=ax-bx;
    lw->dx=-1;
  } else {
    lw->xweight=0;
    lw->dx=0;
  }
  if (ay<by) {
    lw->yweight=ay-by;
    lw->dy=1;
  } else if (ay>by) {
    lw->yweight=by-ay;
    lw->dy=-1;
  } else {
    lw->yweight=0;
    lw->dy=0;
  }
  lw->xthresh=lw->xweight>>1;
  lw->ythresh=lw->yweight>>1;
  lw->weight=lw->xweight+lw->yweight;
  if (texture) {
    lw->stride=texture->w*lw->dy;
    lw->p=texture->v+ay*texture->w+ax;
  } else {
    lw->stride=0;
    lw->p=0;
  }
}

void gfx_linewalker_step(struct gfx_linewalker *lw) {
  if ((lw->x==lw->bx)&&(lw->y==lw->by)) return;
  if (lw->weight>=lw->xthresh) {
    if (lw->x!=lw->bx) {
      lw->x+=lw->dx;
      if (lw->p) lw->p+=lw->dx;
    }
    lw->weight+=lw->yweight;
  } else if (lw->weight<=lw->ythresh) {
    if (lw->y!=lw->by) {
      lw->y+=lw->dy;
      if (lw->p) lw->p+=lw->stride;
    }
    lw->weight+=lw->xweight;
  } else {
    if (lw->x!=lw->bx) {
      lw->x+=lw->dx;
      if (lw->p) lw->p+=lw->dx;
    }
    if (lw->y!=lw->by) {
      lw->y+=lw->dy;
      if (lw->p) lw->p+=lw->stride;
    }
    lw->weight+=lw->xweight+lw->yweight;
  }
}

void gfx_linewalker_step_y(struct gfx_linewalker *lw) {
  if (lw->y!=lw->by) {
    int pvy=lw->y;
    while (lw->y==pvy) gfx_linewalker_step(lw);
  }
  while ((lw->weight>=lw->xthresh)&&(lw->x!=lw->bx)) {
    lw->x+=lw->dx;
    lw->weight+=lw->yweight;
    if (lw->p) lw->p+=lw->dx;
  }
}

void gfx_linewalker_to_row_end(struct gfx_linewalker *lw) {
  while ((lw->weight>=lw->xthresh)&&(lw->x!=lw->bx)) {
    lw->x+=lw->dx;
    lw->weight+=lw->yweight;
    if (lw->p) lw->p+=lw->dx;
  }
}

uint8_t gfx_linewalker_progress_u08(const struct gfx_linewalker *lw) {
  int yw=-lw->yweight;
  if (yw>=lw->xweight) {
    if (yw<1) return 0xff;
    int p=lw->y-lw->ay;
    if (p<0) p=-p;
    return (p*0xff)/yw;
  } else {
    if (lw->xweight<1) return 0xff;
    int p=lw->x-lw->ax;
    if (p<0) p=-p;
    return (p*0xff)/lw->xweight;
  }
}
