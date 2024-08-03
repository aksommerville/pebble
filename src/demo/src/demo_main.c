#include "pebble/pebble.h"
#include "util/rom.h"
#include "gfx/gfx.h"
#include "stdlib/egg-stdlib.h"
#include "image/image.h"
#include "lofi/lofi.h"
#include <stdint.h>

// Must match metadata.
#define SCREENW 160
#define SCREENH 90

static int pvinput=0;
static int16_t synthbuf[1024]; // Can freely change the size here.
static int16_t synth_tone_level=3000; // Can freely change audio level here.
static int synth_tone_halfperiod=12345; // Will recalculate at init, after that it's constant.
static int synth_tone_phase=0; // Counts down.
static int sprite_texid=0;
static int xform_texid=0;
static struct image *fontimage=0; // 1-bit or nothing.

/* Even with only 100 sprites at 8x8, the CPU load is substantial.
 * Noticeably higher when we use "onebit" instead of regular blit.
 */
#define SPRITEC 40
static struct sprite {
  int x,y;
  int dx,dy;
  int color;
  uint8_t xform;
} spritev[SPRITEC];

static const uint8_t onebitsrc[]={
#define px(a,b,c,d,e,f,g,h) (a?0x80:0)|(b?0x40:0)|(c?0x20:0)|(d?0x10:0)|(e?0x08:0)|(f?0x04:0)|(g?0x02:0)|(h?0x01:0),
#define X 1
#define _ 0
  px(_,_,X,X,X,X,_,_)
  px(_,X,_,X,_,_,X,_)
  px(X,_,_,_,_,_,_,X)
  px(X,_,_,_,X,_,_,X)
  px(X,_,_,X,X,_,X,X)
  px(X,_,_,_,_,_,X,X)
  px(_,X,_,_,X,X,X,_)
  px(_,_,X,X,X,X,_,_)
#undef px
#undef X
#undef _
};

void pbl_client_quit() {
  // Really not necessary to quit these subsystems.
  // I'm doing it to ensure that our heap usage return to zero, just for validation.
  rom_quit();
  gfx_quit();
  lofi_quit();
}

int pbl_client_init(int fbw,int fbh,int rate,int chanc) {

  srand_auto();
  if ((fbw!=SCREENW)||(fbh!=SCREENH)) return -1;
  if (rom_init()<0) return -1;
  if (gfx_init(fbw,fbh)<0) return -1;
  if (lofi_init(rate,chanc)<0) return -1;
  
  #define _ "\0\0\0\0"
  #define K "\0\0\0\xff"
  #define W "\xff\xff\xff\xff"
  #define R "\xff\0\0\xff"
  #define G "\0\xff\0\xff"
  #define B "\0\0\xff\xff"
  if ((sprite_texid=gfx_texture_new_rgba(8,8,32,
    _ _ K K K K _ _
    _ K W W R R K _
    K W W R R R R K
    K W G B B B B K
    K G G G B B B K
    K G G G G B B K
    _ K G G G G K _
    _ _ K K K K _ _
  ,256))<0) return -1;
  
  if ((xform_texid=gfx_texture_new_rgba(8,8,32,
    K K K _ _ K K _
    _ K _ _ _ K _ K
    _ K _ K K K K _
    _ K _ K K K _ _
    _ _ _ _ _ _ _ _
    _ G _ _ _ _ R _
    K G _ _ _ _ R K
    K K K K K K K K
  ,256))<0) return -1;
  #undef _
  #undef K
  #undef W
  #undef R
  #undef G
  #undef B
  
  struct sprite *sprite=spritev;
  int i=SPRITEC;
  for (;i-->0;sprite++) {
    sprite->x=rand()%SCREENW;
    sprite->y=rand()%SCREENH;
    sprite->dx=rand()%3-1;
    sprite->dy=rand()%3-1;
    sprite->color=rand();
    sprite->xform=rand()&7;
  }
  
  const char *title=0;
  int titlec=rom_get_metadata(&title,"title",5);
  pbl_log("metadata title = '%.*s'",titlec,title);
  
  // Call all 11 functions of the Pebble Client API.
  pbl_log("Hello I'm logging some text.");
  //pbl_terminate(0); // Well, let's not call *this* one every time...
  pbl_set_synth_limit(sizeof(synthbuf)/sizeof(synthbuf[0]));
  pbl_log("pbl_now_real: %f",pbl_now_real());
  int ltime[7]={0};
  pbl_now_local(ltime,sizeof(ltime)/sizeof(ltime[0]));
  pbl_log("pbl_now_local: %d-%d-%dT%d:%d:%d.%d",ltime[0],ltime[1],ltime[2],ltime[3],ltime[4],ltime[5],ltime[6]);
  pbl_log("pbl_get_global_language: %d",pbl_get_global_language());
  pbl_set_global_language(PBL_LANG_FROM_STRING("fr")); // This we can do, but it's astonishingly rude of us.
  pbl_begin_input_config(0); // We shouldn't do this either, at least not once it starts working.
  char str[256];
  int strc=pbl_store_get(str,sizeof(str),"savedGame",9);
  pbl_log("pbl_store_get('savedGame'): '%.*s'",strc,str);
  pbl_log("pbl_store_set: %d",pbl_store_set("savedGame",9,"abcdefg",7));
  strc=pbl_store_key_by_index(str,sizeof(str),0);
  pbl_log("pbl_store_key_by_index(0): '%.*s'",strc,str);
  
  pbl_log("Image decoder test...");
  int imageid=2;
  const void *serial=0;
  int serialc=rom_get(&serial,PBL_TID_image,imageid);
  pbl_log("image:%d size=%d",imageid,serialc);
  struct image *image=image_decode(serial,serialc);
  if (!image) {
    pbl_log("DECODE FAILED!");
  } else {
    pbl_log("Decode ok. v=%p w=%d h=%d stride=%d pixelsize=%d",image->v,image->w,image->h,image->stride,image->pixelsize);
    if (image->pixelsize==1) {
      fontimage=image;
    } else {
      image_del(image);
    }
  }
  
  lofi_wave_init_sine(0);
  lofi_wave_init_square(1);
  lofi_wave_init_saw(2);
  lofi_wave_init_triangle(3);
  { uint8_t coefv[]={0x80,0x40,0x20,0x10,0x08}; lofi_wave_init_harmonics(4,coefv,sizeof(coefv)); }
  { uint8_t coefv[]={0xa0,0x00,0x40,0x00,0x10}; lofi_wave_init_harmonics(5,coefv,sizeof(coefv)); }
  { uint8_t coefv[]={0x40,0x50,0x30,0x10,0x08,0x10,0x08,0x10}; lofi_wave_init_harmonics(6,coefv,sizeof(coefv)); }
  { uint8_t coefv[]={0xff,0xc0,0x80,0x40,0x20,0x10}; lofi_wave_init_harmonics(7,coefv,sizeof(coefv)); }
  
  {
    serialc=rom_get(&serial,PBL_TID_song,1);
    //lofi_play_song(serial,serialc);
  }

  return 0;
}

void pbl_client_update(double elapsed,int in1,int in2,int in3,int in4) {
  int input=in1|in2|in3|in4;
  if (input!=pvinput) {
    pbl_log(
      "INPUT STATE: 0x%x %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
      input,
      #define _(tag) (input&PBL_BTN_##tag)?" "#tag:""
      _(LEFT),
      _(RIGHT),
      _(UP),
      _(DOWN),
      _(SOUTH),
      _(WEST),
      _(EAST),
      _(NORTH),
      _(L1),
      _(R1),
      _(L2),
      _(R2),
      _(AUX1),
      _(AUX2),
      _(AUX3),
      _(CD)
      #undef _
    );
    #define PRESS(tag) ((input&PBL_BTN_##tag)&&!(pvinput&PBL_BTN_##tag))
    uint8_t program=(
      ( 0 ?0x80:0)| // sharp attack
      ( 0 <<5)| // attack time 0..3
      ( 0 <<3)| // release time 0..3
      7 // wave id 0..7
    );
    int durms=50;
    if (PRESS(UP   )) lofi_note(program,0x80,0x30,0x20,durms);
    if (PRESS(DOWN )) lofi_note(program,0x80,0x41,0x50,durms);
    if (PRESS(LEFT )) lofi_note(program,0x80,0x42,0x50,durms);
    if (PRESS(RIGHT)) lofi_note(program,0x80,0x43,0x50,durms);
    if (PRESS(SOUTH)) lofi_note(program,0x80,0x44,0x50,durms);
    if (PRESS(WEST )) lofi_note(program,0x80,0x45,0x50,durms);
    if (PRESS(EAST )) lofi_note(program,0x80,0x46,0x30,durms);
    if (PRESS(NORTH)) lofi_note(program,0x80,0x47,0x30,durms);
    if (PRESS(L1   )) lofi_note(program,0x80,0x48,0x30,durms);
    if (PRESS(R1   )) lofi_note(program,0x80,0x49,0x30,durms);
    if (PRESS(L2   )) lofi_note(program,0x80,0x4a,0x30,durms);
    if (PRESS(R2   )) lofi_note(program,0x80,0x4b,0x30,durms);
    if (PRESS(AUX1 )) lofi_note(program,0x80,0x4c,0x30,durms);
    if (PRESS(AUX2 )) lofi_note(program,0x80,0x4d,0x30,durms);
    if (PRESS(AUX3 )) lofi_note(program,0x80,0x20,0x28,durms);
    #undef PRESS
    pvinput=input;
  }
  
  //TODO Any but the most trivial demos really ought to use floating-point coords, and multiply velocities by (elapsed).
  struct sprite *sprite=spritev;
  int i=SPRITEC;
  for (;i-->0;sprite++) {
    sprite->x+=sprite->dx; if ((sprite->x<0)&&(sprite->dx<0)) sprite->dx=-sprite->dx; else if ((sprite->x>=SCREENW)&&(sprite->dx>0)) sprite->dx=-sprite->dx;
    sprite->y+=sprite->dy; if ((sprite->y<0)&&(sprite->dy<0)) sprite->dy=-sprite->dy; else if ((sprite->y>=SCREENH)&&(sprite->dy>0)) sprite->dy=-sprite->dy;
  }
}

void *pbl_client_render() {
  gfx_clear(0,0x503038);

  /* Rectangles. Easy, fine.
   *
  gfx_fill_rect(0,10,10,SCREENW-20,SCREENH-20,0xff008000);
  gfx_trace_rect(0,8,8,SCREENW-16,SCREENH-16,0xff00ffff);
  /**/

  /* Blits (toggle regular or one-bit recoloring). OK
   *
  struct sprite *sprite=spritev;
  int i=SPRITEC;
  for (;i-->0;sprite++) {
    //gfx_blit(0,sprite_texid,sprite->x-4,sprite->y-4,0,0,-1,-1,sprite->xform);
    gfx_blit_onebit(0,sprite->x-4,sprite->y-4,onebitsrc,1,8,8,0,0,-1,-1,sprite->xform,sprite->color);
  }
  /**/
  
  /* Validate transforms. OK
   *
  gfx_blit(0,xform_texid,12,12,0,0,-1,-1,0);
  gfx_blit(0,xform_texid,24,12,0,0,-1,-1,GFX_XFORM_XREV);
  gfx_blit(0,xform_texid,36,12,0,0,-1,-1,GFX_XFORM_YREV);
  gfx_blit(0,xform_texid,48,12,0,0,-1,-1,GFX_XFORM_XREV|GFX_XFORM_YREV);
  gfx_blit(0,xform_texid,60,12,0,0,-1,-1,GFX_XFORM_SWAP);
  gfx_blit(0,xform_texid,72,12,0,0,-1,-1,GFX_XFORM_SWAP|GFX_XFORM_XREV);
  gfx_blit(0,xform_texid,84,12,0,0,-1,-1,GFX_XFORM_SWAP|GFX_XFORM_YREV);
  gfx_blit(0,xform_texid,96,12,0,0,-1,-1,GFX_XFORM_SWAP|GFX_XFORM_XREV|GFX_XFORM_YREV);
  gfx_blit_onebit(0,12,24,onebitsrc,1,8,8,0,0,-1,-1,0,0xffff00);
  gfx_blit_onebit(0,24,24,onebitsrc,1,8,8,0,0,-1,-1,GFX_XFORM_XREV,0xffff00);
  gfx_blit_onebit(0,36,24,onebitsrc,1,8,8,0,0,-1,-1,GFX_XFORM_YREV,0xffff00);
  gfx_blit_onebit(0,48,24,onebitsrc,1,8,8,0,0,-1,-1,GFX_XFORM_XREV|GFX_XFORM_YREV,0xffff00);
  gfx_blit_onebit(0,60,24,onebitsrc,1,8,8,0,0,-1,-1,GFX_XFORM_SWAP,0xffff00);
  gfx_blit_onebit(0,72,24,onebitsrc,1,8,8,0,0,-1,-1,GFX_XFORM_SWAP|GFX_XFORM_XREV,0xffff00);
  gfx_blit_onebit(0,84,24,onebitsrc,1,8,8,0,0,-1,-1,GFX_XFORM_SWAP|GFX_XFORM_YREV,0xffff00);
  gfx_blit_onebit(0,96,24,onebitsrc,1,8,8,0,0,-1,-1,GFX_XFORM_SWAP|GFX_XFORM_XREV|GFX_XFORM_YREV,0xffff00);
  /**/
  
  /* Line. OK
   *
  gfx_trace_line(0,1,1,SCREENW-2,SCREENH-2,0xffffff);
  gfx_trace_line(0,1,SCREENH-2,SCREENW-2,1,0xffffff);
  gfx_trace_line(0,3,1,SCREENW-4,1,0xffffff);
  gfx_trace_line(0,1,3,1,SCREENH-4,0xffffff);
  /**/
  
  /* Triangle. OK
   * Mind that there are lots of edge cases we didn't look at.
   * This was copied from eggsc, and I validated pretty hard that first time.
   *
  if (SPRITEC>=3) {
    gfx_fill_trig(0,
      spritev[0].x,spritev[0].y,
      spritev[1].x,spritev[1].y,
      spritev[2].x,spritev[2].y,
      0x004080
    );
  }
  /**/
  
  /* Oval. OK
   *
  gfx_fill_oval(0,10,10,SCREENW-20,SCREENH-20,0x808080);
  gfx_fill_oval(0,10,10,SCREENW-21,SCREENH-21,0xffffff);
  gfx_trace_oval(0,10,10,SCREENW-20,SCREENH-20,0x00ffff);
  /**/
  
  if (fontimage) {
    gfx_blit_onebit(0,5,5,fontimage->v,fontimage->stride,fontimage->w,fontimage->h,0,0,-1,-1,0,0xff8040);
  }
   
  return gfx_finish();
}

void *pbl_client_synth(int samplec) {
  lofi_update(synthbuf,samplec);
  return synthbuf;
}
