#include "gfx_internal.h"

/* Fill rect in resolved texture.
 * Caller finalizes (xbgr).
 * We take care of clipping.
 */
 
static void gfx_texture_fill_rect(struct gfx_texture *dsttex,int x,int y,int w,int h,int xbgr) {
  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  if (x>dsttex->w-w) w=dsttex->w-x;
  if (y>dsttex->h-h) h=dsttex->h-y;
  if ((w<1)||(h<1)) return;
  int *dstrow=dsttex->v+y*dsttex->w+x;
  for (;h-->0;dstrow+=dsttex->w) {
    int *dstp=dstrow;
    int xi=w; for (;xi-->0;dstp++) *dstp=xbgr;
  }
}

/* Clear.
 */
 
void gfx_clear(int dsttexid,int abgr) {
  if ((dsttexid<0)||(dsttexid>=gfx.texturec)) return;
  struct gfx_texture *dsttex=gfx.texturev+dsttexid;
  if (!dsttexid) abgr|=0xff000000;
  int *v=dsttex->v;
  int i=dsttex->w*dsttex->h;
  for (;i-->0;v++) *v=abgr;
}

/* Trace line.
 */

void gfx_trace_line(int dsttexid,int ax,int ay,int bx,int by,int xbgr) {
  if ((dsttexid<0)||(dsttexid>=gfx.texturec)) return;
  struct gfx_texture *dsttex=gfx.texturev+dsttexid;
  if (!dsttexid) xbgr|=0xff000000;
  struct gfx_linewalker lw;
  gfx_linewalker_init(&lw,ax,ay,bx,by,dsttex);
  // In the likely event that both points are in bounds, no need for a bounds check at each pixel.
  if ((ax>=0)&&(ay>=0)&&(bx>=0)&&(by>=0)&&(ax<dsttex->w)&&(ay<dsttex->h)&&(bx<dsttex->w)&&(by<dsttex->h)) {
    for (;;) {
      *(lw.p)=xbgr;
      if ((lw.x==bx)&&(lw.y==by)) break;
      gfx_linewalker_step(&lw);
    }
  } else {
    for (;;) {
      if ((lw.x>=0)&&(lw.y>=0)&&(lw.x<dsttex->w)&&(lw.y<dsttex->h)) {
        *(lw.p)=xbgr;
      }
      if ((lw.x==bx)&&(lw.y==by)) break;
      gfx_linewalker_step(&lw);
    }
  }
}

/* Trace rect.
 */
 
void gfx_trace_rect(int dsttexid,int x,int y,int w,int h,int xbgr) {
  if ((dsttexid<0)||(dsttexid>=gfx.texturec)) return;
  if ((w<1)||(h<1)) return;
  struct gfx_texture *dsttex=gfx.texturev+dsttexid;
  if (!dsttexid) xbgr|=0xff000000;
  gfx_texture_fill_rect(dsttex,x,y,1,h,xbgr);
  gfx_texture_fill_rect(dsttex,x,y,w,1,xbgr);
  gfx_texture_fill_rect(dsttex,x+w-1,y,1,h,xbgr);
  gfx_texture_fill_rect(dsttex,x,y+h-1,w,1,xbgr);
}

/* Fill rect.
 */
 
void gfx_fill_rect(int dsttexid,int x,int y,int w,int h,int xbgr) {
  if ((dsttexid<0)||(dsttexid>=gfx.texturec)) return;
  struct gfx_texture *dsttex=gfx.texturev+dsttexid;
  if (!dsttexid) xbgr|=0xff000000;
  gfx_texture_fill_rect(dsttex,x,y,w,h,xbgr);
}

/* Trace trig.
 */
 
void gfx_trace_trig(int dsttexid,int ax,int ay,int bx,int by,int cx,int cy,int xbgr) {
  gfx_trace_line(dsttexid,ax,ay,bx,by,xbgr);
  gfx_trace_line(dsttexid,bx,by,cx,cy,xbgr);
  gfx_trace_line(dsttexid,cx,cy,ax,ay,xbgr);
}

/* Fill trig.
 */
 
void gfx_fill_trig(int dsttexid,int ax,int ay,int bx,int by,int cx,int cy,int xbgr) {
  if ((dsttexid<0)||(dsttexid>=gfx.texturec)) return;
  struct gfx_texture *dsttex=gfx.texturev+dsttexid;
  if (!dsttexid) xbgr|=0xff000000;
  
  // Sort the three points vertically, (a) on top.
  #define SWAP(first,second) { \
    int tmp=first##x; \
    first##x=second##x; \
    second##x=tmp; \
    tmp=first##y; \
    first##y=second##y; \
    second##y=tmp; \
  }
  if (ay>by) SWAP(a,b)
  if (by>cy) {
    SWAP(b,c)
    if (ay>by) SWAP(a,b)
  }
  #undef SWAP
  
  // Our algorithm can miss pixels when two points are both at the top row.
  // Detect this case and fill that row explicitly.
  // And if all three are there, we're done.
  if (ay==by) {
    if (ay==cy) {
      int l,r;
      if ((ax<=bx)&&(ax<=cx)) l=ax;
      else if (bx<=cx) l=bx;
      else l=cx;
      if ((ax>=bx)&&(ax>=cx)) r=ax;
      else if (bx>=cx) r=bx;
      else r=cx;
      int *p=dsttex->v+ay*dsttex->w+l;
      for (r=r-l+1;r-->0;p++) *p=xbgr;
      return;
    }
    int l,r;
    if (ax<=bx) { l=ax; r=bx; }
    else { l=bx; r=ax; }
    int *p=dsttex->v+ay*dsttex->w+l;
    for (r=r-l+1;r-->0;p++) *p=xbgr;
  }
  
  // Prepare two line walkers, both start at (a).
  struct gfx_linewalker lwb,lwc;
  gfx_linewalker_init(&lwb,ax,ay,bx,by,dsttex);
  gfx_linewalker_init(&lwc,ax,ay,cx,cy,dsttex);
  
  // Fill in the current row, or terminate if we exceed the bottom.
  // TODO Optimizations around bounds checking are certainly possible.
  #define VISIT_ROW { \
    if (lwb.y>=dsttex->h) return; \
    if (lwb.y>=0) { \
      int x,w; \
      if (lwb.x<=lwc.x) { \
        x=lwb.x; \
        w=lwc.x-lwb.x+1; \
      } else { \
        x=lwc.x; \
        w=lwb.x-lwc.x+1; \
      } \
      if (x<0) { w+=x; x=0; } \
      if (x>dsttex->w-w) w=dsttex->w-x; \
      if (w>0) { \
        int *p=dsttex->v+lwb.y*dsttex->w+x; \
        for (;w-->0;p++) *p=xbgr; \
      } \
    } \
  }
  
  // Extend each walker as far as it will go on the current row, and fill in that row.
  gfx_linewalker_to_row_end(&lwb);
  gfx_linewalker_to_row_end(&lwc);
  VISIT_ROW
  
  // Step both walkers rowwise until we reach (b).
  while (lwb.y!=by) {
    gfx_linewalker_step_y(&lwb);
    gfx_linewalker_step_y(&lwc);
    VISIT_ROW
  }
  
  // Reinitialize walker (b) to travel from (b) to (c).
  // No need for an initial x-extend and visit this time; we already visited this row.
  gfx_linewalker_init(&lwb,bx,by,cx,cy,dsttex);
  
  // Proceed until they both reach (c). We only need to check one; their (y) stay in sync.
  for (;;) {
    gfx_linewalker_step_y(&lwb);
    gfx_linewalker_step_y(&lwc);
    VISIT_ROW
    if (lwb.y==cy) break;
  }
  #undef VISIT_ROW
}

/* Trace oval.
 */
 
void gfx_trace_oval(int dsttexid,int x,int y,int w,int h,int xbgr) {

  // Like fill-oval, this collapses to fill-rect at 2 pixels or smaller.
  if ((w<=2)||(h<=2)) {
    gfx_fill_rect(dsttexid,x,y,w,h,xbgr);
    return;
  }
  
  if ((dsttexid<0)||(dsttexid>=gfx.texturec)) return;
  struct gfx_texture *dsttex=gfx.texturev+dsttexid;
  if (!dsttexid) xbgr|=0xff000000;
  int halfw=w>>1,halfh=h>>1;
  int midx=x+halfw;
  
  #define VISIT_ROW(y,linew) { \
    gfx_texture_fill_rect(dsttex,midx-halfw,y,linew,1,xbgr); \
    gfx_texture_fill_rect(dsttex,midx+halfw-linew,y,linew,1,xbgr); \
  }
  
  /* Iterate exactly as we do for fill-oval (see notes there).
   * Well, not exactly. The final row is an edge case; we pick it off special.
   */
  int ytop=y+halfh;
  int ybottom=ytop;
  if (h&1) {
    VISIT_ROW(ytop,1)
    ytop--;
    ybottom++;
  } else {
    ytop--;
  }
  halfh--;
  int wadd=w&1;
  int yscale=((halfw*halfw)<<15)/(halfh*halfh);
  int r2=halfw*halfw;
  int x2=halfw*halfw,x2d=(halfw<<1)+1,y2=1,y2d=1;
  int x2next=x2-x2d;
  for (;ytop>y;ytop--,ybottom++) {
    y2+=y2d;
    y2d+=2;
    int linew=0;
    while (x2next+((y2*yscale)>>15)>r2) {
      x2=x2next;
      x2next-=x2d;
      x2d-=2;
      linew++;
    }
    if (linew) {
      VISIT_ROW(ytop,linew)
      VISIT_ROW(ybottom,linew)
      halfw-=linew;
    } else {
      VISIT_ROW(ytop,1)
      VISIT_ROW(ybottom,1)
    }
  }
  gfx_texture_fill_rect(dsttex,midx-halfw,ytop,(halfw<<1)+wadd,1,xbgr);
  gfx_texture_fill_rect(dsttex,midx-halfw,ybottom,(halfw<<1)+wadd,1,xbgr);
  #undef VISIT_ROW
}

/* Fill oval.
 */
 
void gfx_fill_oval(int dsttexid,int x,int y,int w,int h,int xbgr) {

  // Either axis 2 pixels or shorter, fill rect instead. This is important, we have a dangerous edge case at 2 pixels.
  if ((w<=2)||(h<=2)) {
    gfx_fill_rect(dsttexid,x,y,w,h,xbgr);
    return;
  }

  if ((dsttexid<0)||(dsttexid>=gfx.texturec)) return;
  struct gfx_texture *dsttex=gfx.texturev+dsttexid;
  if (!dsttexid) xbgr|=0xff000000;
  int halfw=w>>1,halfh=h>>1;
  int midx=x+halfw;
  
  // We'll iterate rows from center outward. Ovals being symmetric, each pair of rows is the same.
  // If we have odd height, fill the center row.
  int ytop=y+halfh;
  int ybottom=ytop;
  if (h&1) {
    gfx_texture_fill_rect(dsttex,x,ytop,w,1,xbgr);
    ytop--;
    ybottom++;
  } else {
    ytop--;
  }
  
  /* Take the basic circle equation: x2+y2<=r2
   * The perfect squares are separated by sequential odd numbers.
   * Since we're iterating both x and y, we can exploit that, and advance x2 and y2 by simple counters.
   * An extra "yscale" allows for deformation along either axis.
   */
  int wadd=w&1;
  int yscale=((halfw*halfw)<<15)/(halfh*halfh);
  int r2=halfw*halfw;
  int x2=halfw*halfw,x2d=(halfw<<1)+1,y2=1,y2d=1;
  int x2next=x2-x2d;
  for (;ytop>=y;ytop--,ybottom++) {
  
    y2+=y2d;
    y2d+=2;
    while (x2next+((y2*yscale)>>15)>r2) {
      x2=x2next;
      x2next-=x2d;
      x2d-=2;
      halfw--;
    }
    
    int fullw=(halfw<<1)+wadd;
    gfx_texture_fill_rect(dsttex,midx-halfw,ytop,fullw,1,xbgr);
    gfx_texture_fill_rect(dsttex,midx-halfw,ybottom,fullw,1,xbgr);
  }
}

/* Blit.
 */

void gfx_blit(
  int dsttexid,int srctexid,
  int dstx,int dsty,
  int srcx,int srcy,
  int w,int h,
  int xform
) {
  if (dsttexid==srctexid) return; // Explicitly forbidden.
  if ((dsttexid<0)||(dsttexid>=gfx.texturec)) return;
  struct gfx_texture *dsttex=gfx.texturev+dsttexid;
  if ((srctexid<0)||(srctexid>=gfx.texturec)) return;
  struct gfx_texture *srctex=gfx.texturev+srctexid;
  
  /* Extend (w,h) if negative, then reject the whole op if OOB on src.
   */
  if (w<0) w=srctex->w-srcx;
  if (h<0) h=srctex->h-srcy;
  if ((w<1)||(srcx<0)||(srcx>srctex->w-w)) return;
  if ((h<1)||(srcy<0)||(srcy>srctex->h-h)) return;
  
  /* Optimize for no transform. That's a common case, and very easy to do.
   * When transformed, we could pre-clip but it's monstrously complicated so we don't.
   */
  if (!xform) {
    if (dstx<0) { srcx-=dstx; w+=dstx; dstx=0; }
    if (dsty<0) { srcy-=dsty; h+=dsty; dsty=0; }
    if (dstx>dsttex->w-w) w=dsttex->w-dstx;
    if (dsty>dsttex->h-h) h=dsttex->h-dsty;
    if ((w<1)||(h<1)) return;
    int *dstrow=dsttex->v+dsttex->w*dsty+dstx;
    const int *srcrow=srctex->v+srctex->w*srcy+srcx;
    for (;h-->0;dstrow+=dsttex->w,srcrow+=srctex->w) {
      int *dstp=dstrow;
      const int *srcp=srcrow;
      int xi=w; for (;xi-->0;dstp++,srcp++) {
        if (*srcp) *dstp=*srcp;
      }
    }
    return;
  }
  
  /* Transformed but fully in bounds, we can use the more efficient gfx_iter.
   * Be careful about swapped w/h.
   */
  int dstw,dsth;
  if (xform&GFX_XFORM_SWAP) {
    dstw=h;
    dsth=w;
  } else {
    dstw=w;
    dsth=h;
  }
  if ((dstx>=0)&&(dsty>=0)&&(dstx<=dsttex->w-dstw)&&(dsty<=dsttex->h-dsth)) {
    struct gfx_iter dstiter,srciter;
    if (gfx_iter_init(&dstiter,dsttex,dstx,dsty,dstw,dsth,xform&GFX_XFORM_SWAP)<0) return;
    if (gfx_iter_init(&srciter,srctex,srcx,srcy,w,h,xform&(GFX_XFORM_XREV|GFX_XFORM_YREV))<0) return;
    do {
      if (*(srciter.p)) *(dstiter.p)=*(srciter.p);
    } while (gfx_iter_next(&dstiter)&&gfx_iter_next(&srciter));
  }
  
  /* Transformed and possibly OOB. Must use gfx_safeiter for dst.
   */
  struct gfx_safeiter dstiter;
  struct gfx_iter srciter;
  if (gfx_safeiter_init(&dstiter,dsttex,dstx,dsty,dstw,dsth,xform&GFX_XFORM_SWAP)<0) return;
  if (gfx_iter_init(&srciter,srctex,srcx,srcy,w,h,xform&(GFX_XFORM_XREV|GFX_XFORM_YREV))<0) return;
  do {
    if (*(srciter.p)&&gfx_safeiter_in_bounds(&dstiter)) {
      dsttex->v[dsttex->w*dstiter.y+dstiter.x]=*(srciter.p);
    }
  } while (gfx_safeiter_next(&dstiter)&&gfx_iter_next(&srciter));
}

/* Blit from 1-bit.
 */

void gfx_blit_onebit(
  int dsttexid,int dstx,int dsty,
  const void *src,int srcstride,int srcw,int srch,
  int srcx,int srcy,
  int w,int h,
  int xform,
  int xbgr
) {
  if ((dsttexid<0)||(dsttexid>=gfx.texturec)) return;
  struct gfx_texture *dsttex=gfx.texturev+dsttexid;
  if (!src||(srcstride<1)||(srcw<1)||(srch<1)) return;
  if (!dsttexid) xbgr|=0xff000000;
  
  /* Extend (w,h) if negative, then reject the whole op if OOB on src.
   */
  if (w<0) w=srcw-srcx;
  if (h<0) h=srch-srcy;
  if ((w<1)||(srcx<0)||(srcx>srcw-w)) return;
  if ((h<1)||(srcy<0)||(srcy>srch-h)) return;
  
  /* Optimize for no transform. That's a common case, and very easy to do.
   * When transformed, we could pre-clip but it's monstrously complicated so we don't.
   */
  if (!xform) {
    if (dstx<0) { srcx-=dstx; w+=dstx; dstx=0; }
    if (dsty<0) { srcy-=dsty; h+=dsty; dsty=0; }
    if (dstx>dsttex->w-w) w=dsttex->w-dstx;
    if (dsty>dsttex->h-h) h=dsttex->h-dsty;
    if ((w<1)||(h<1)) return;
    int *dstrow=dsttex->v+dsttex->w*dsty+dstx;
    const unsigned char *srcrow=src;
    srcrow+=srcstride*srcy+(srcx>>3);
    unsigned char srcmask0=0x80>>(srcx&7);
    for (;h-->0;dstrow+=dsttex->w,srcrow+=srcstride) {
      int *dstp=dstrow;
      const unsigned char *srcp=srcrow;
      unsigned char srcmask=srcmask0;
      int xi=w; for (;xi-->0;dstp++) {
        if ((*srcp)&srcmask) *dstp=xbgr;
        if (srcmask==1) { srcmask=0x80; srcp++; }
        else srcmask>>=1;
      }
    }
    return;
  }
  
  /* Transformed but fully in bounds, we can use the more efficient gfx_iter.
   * Be careful about swapped w/h.
   * Because we effect the transform entirely in the dst iterator, XREV and YREV get reversed with SWAP.
   */
  int dstw,dsth;
  if (xform&GFX_XFORM_SWAP) {
    dstw=h;
    dsth=w;
    xform=GFX_XFORM_SWAP|((xform&GFX_XFORM_XREV)?GFX_XFORM_YREV:0)|((xform&GFX_XFORM_YREV)?GFX_XFORM_XREV:0);
  } else {
    dstw=w;
    dsth=h;
  }
  if ((dstx>=0)&&(dsty>=0)&&(dstx<=dsttex->w-dstw)&&(dsty<=dsttex->h-dsth)) {
    struct gfx_iter dstiter;
    if (gfx_iter_init(&dstiter,dsttex,dstx,dsty,dstw,dsth,xform)<0) return;
    const unsigned char *srcrow=src;
    srcrow+=srcstride*srcy+(srcx>>3);
    unsigned char srcmask0=0x80>>(srcx&7);
    for (;h-->0;srcrow+=srcstride) {
      const unsigned char *srcp=srcrow;
      unsigned char srcmask=srcmask0;
      int xi=w; for (;xi-->0;) {
        if ((*srcp)&srcmask) *(dstiter.p)=xbgr;
        if (!gfx_iter_next(&dstiter)) return;
        if (srcmask==1) { srcmask=0x80; srcp++; }
        else srcmask>>=1;
      }
    }
  }
  
  /* Transformed and possibly OOB. Must use gfx_safeiter for dst.
   */
  struct gfx_safeiter dstiter;
  if (gfx_safeiter_init(&dstiter,dsttex,dstx,dsty,dstw,dsth,xform&GFX_XFORM_SWAP)<0) return;
  const unsigned char *srcrow=src;
  srcrow+=srcstride*srcy+(srcx>>3);
  unsigned char srcmask0=0x80>>(srcx&7);
  for (;h-->0;srcrow+=srcstride) {
    const unsigned char *srcp=srcrow;
    unsigned char srcmask=srcmask0;
    int xi=w; for (;xi-->0;) {
      if ((*srcp)&srcmask) {
        if (gfx_safeiter_in_bounds(&dstiter)) {
          dsttex->v[dsttex->w*dstiter.y+dstiter.x]=xbgr;
        }
      }
      if (!gfx_safeiter_next(&dstiter)) return;
      if (srcmask==1) { srcmask=0x80; srcp++; }
      else srcmask>>=1;
    }
  }
}
