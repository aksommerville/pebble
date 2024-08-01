#include "gfx_internal.h"

struct gfx gfx={0};

/* Quit.
 */
 
static void gfx_texture_cleanup(struct gfx_texture *tex) {
  if (tex->v) free(tex->v);
}
 
void gfx_quit() {
  if (gfx.texturev) {
    while (gfx.texturec-->0) gfx_texture_cleanup(gfx.texturev+gfx.texturec);
    free(gfx.texturev);
  }
  memset(&gfx,0,sizeof(gfx));
}

/* Init.
 */
 
int gfx_init(int fbw,int fbh) {
  if (gfx.texturec>0) {
    if ((gfx.texturev[0].w!=fbw)||(gfx.texturev[0].h!=fbh)) return -1;
    return 0;
  }
  if (gfx_texture_new(fbw,fbh)!=0) {
    gfx_quit();
    return -1;
  }
  return 0;
}

/* Delete texture.
 */
 
void gfx_texture_del(int texid) {
  if ((texid<1)||(texid>gfx.texturec)) return; // Deleting [0] is not allowed.
  struct gfx_texture *tex=gfx.texturev+texid;
  gfx_texture_cleanup(tex);
  memset(tex,0,sizeof(struct gfx_texture));
}

/* New texture.
 */
 
static struct gfx_texture *gfx_texture_alloc() {
  if (gfx.texturec<gfx.texturea) {
    struct gfx_texture *tex=gfx.texturev+gfx.texturec++;
    memset(tex,0,sizeof(struct gfx_texture));
    return tex;
  }
  struct gfx_texture *tex=gfx.texturev;
  int i=gfx.texturec;
  for (;i-->0;tex++) {
    if (!tex->v) return tex;
  }
  if (gfx.texturec>=GFX_TEXTURE_COUNT_LIMIT) return 0;
  int na=gfx.texturea+8;
  if (na>INT_MAX/sizeof(struct gfx_texture)) return 0;
  void *nv=realloc(gfx.texturev,sizeof(struct gfx_texture)*na);
  if (!nv) return 0;
  gfx.texturev=nv;
  gfx.texturea=na;
  tex=gfx.texturev+gfx.texturec++;
  memset(tex,0,sizeof(struct gfx_texture));
  return tex;
}
 
int gfx_texture_new(int w,int h) {
  if ((w<1)||(w>GFX_TEXTURE_SIZE_LIMIT)) return -1;
  if ((h<1)||(h>GFX_TEXTURE_SIZE_LIMIT)) return -1;
  struct gfx_texture *tex=gfx_texture_alloc();
  if (!tex) return -1;
  if (!(tex->v=calloc(w*h,4))) return -1;
  tex->w=w;
  tex->h=h;
  return tex-gfx.texturev;
}

/* New texture with provided pixels.
 */

int gfx_texture_new_rgba(int w,int h,int stride,const void *src,int srcc) {
  if ((w<1)||(w>GFX_TEXTURE_SIZE_LIMIT)) return -1;
  if ((h<1)||(h>GFX_TEXTURE_SIZE_LIMIT)) return -1;
  if (stride<1) stride=w<<2;
  else if (stride<(w<<2)) return -1;
  if (stride>INT_MAX/h) return -1;
  if (stride&3) return -1;
  if (srcc<stride*h) return -1;
  int *nv=malloc(stride*h);
  if (!nv) return -1;
  struct gfx_texture *tex=gfx_texture_alloc();
  if (!tex) {
    free(nv);
    return -1;
  }
  tex->v=nv;
  tex->w=w;
  tex->h=h;
  int *dstrow=tex->v;
  const int *srcrow=src;
  int srcstride=stride>>2;
  int cpc=w<<2;
  for (;h-->0;dstrow+=w,srcrow+=srcstride) {
    memcpy(dstrow,srcrow,cpc);
  }
  return tex-gfx.texturev;
}

/* Finish frame.
 */

void *gfx_finish() {
  if (gfx.texturec<1) return 0;
  return gfx.texturev[0].v;
}
