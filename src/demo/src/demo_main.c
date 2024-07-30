#include "pebble/pebble.h"
#include <stdint.h>

// Must match metadata.
#define SCREENW 160
#define SCREENH 90

static uint8_t rom[8192]={0};
static int romc=0;
static uint8_t fb[SCREENW*SCREENH*4]={0};
static int pvinput=0;
static int16_t synthbuf[1024]; // Can freely change the size here.
static int16_t synth_tone_level=3000; // Can freely change audio level here.
static int synth_tone_halfperiod=12345; // Will recalculate at init, after that it's constant.
static int synth_tone_phase=0; // Counts down.

void pbl_client_quit() {
}

int pbl_client_init(int fbw,int fbh,int rate,int chanc) {
  if ((fbw!=SCREENW)||(fbh!=SCREENH)) return -1;
  if ((romc=pbl_rom_get(rom,sizeof(rom)))>sizeof(rom)) {
    pbl_log("Failed to acquire ROM in Wasm app. length=%d available=%d",romc,(int)sizeof(rom));
    return -1;
  }
  pbl_log("Validate ROM: %x %x %x %x %x %x %x %x",rom[0],rom[1],rom[2],rom[3],rom[4],rom[5],rom[6],rom[7]);
  
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
  
  const int pitchhz=440;
  synth_tone_halfperiod=rate/(pitchhz*2);
  if (synth_tone_halfperiod<1) synth_tone_halfperiod=1;

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
    pvinput=input;
  }
}

void *pbl_client_render() {
  // red,green,blue in the top left corner, to validate.
  uint8_t *v=fb+3;
  int i=SCREENW*SCREENH;
  for (;i-->0;v+=4) *v=0xff;
  fb[0]=0xff;
  fb[5]=0xff;
  fb[10]=0xff;
  return fb;
}

void *pbl_client_synth(int samplec) {
  return 0; // It's not pretty to listen to. Disabling by default.
  // !!! We're assuming that we got the desired mono output. Not necessarily true, in fact my test setup gives us stereo anyway.
  // It's allowed to do that.
  // The only problem getting stereo instead of mono, is our pitch emits an octave higher than expected. Whatever.
  int16_t *dst=synthbuf;
  while (samplec>0) {
    if (synth_tone_phase<=0) {
      synth_tone_phase=synth_tone_halfperiod;
      synth_tone_level=-synth_tone_level;
    }
    int cpc=synth_tone_phase;
    if (cpc>samplec) cpc=samplec;
    samplec-=cpc;
    synth_tone_phase-=cpc;
    for (;cpc-->0;dst++) *dst=synth_tone_level;
  }
  return synthbuf;
}

void *memset(void *dst,int v,unsigned long c) {
  uint8_t *dstp=dst;
  for (;c-->0;dstp++) *dstp=v;
  return dst;
}

void *memcpy(void *dst,const void *src,unsigned long c) {
  uint8_t *dstp=dst;
  const uint8_t *srcp=src;
  for (;c-->0;dstp++,srcp++) *dstp=*srcp;
  return dst;
}
