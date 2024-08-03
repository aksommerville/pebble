#include "lofi_internal.h"

struct lofi lofi={0};

/* Quit.
 */

void lofi_quit() {
  memset(&lofi,0,sizeof(lofi));
}

/* Populate lofi.step_by_noteid.
 */
 
static void lofi_calculate_steps() {

  // Calculate the lowest octave in normalized rates.
  // We'll multiply by 2**32 to get steps out of these.
  // The first is hard-coded against a rate of 8.175798915643702 Hz.
  // The other eleven, we get by multiplying by the twelfth root of two.
  double lows[12];
  lows[0]=8.175798915643702/(double)lofi.rate;
  int i=1; for (;i<12;i++) lows[i]=lows[i-1]*1.0594630943592953;
  
  // Now express that lowest octave as u0.32, and the first twelve are ready.
  for (i=0;i<12;i++) lofi.step_by_noteid[i]=(uint32_t)(4294967296.0*lows[i]);
  
  // And for the rest of the range, double the rate 12 semitones below.
  for (;i<128;i++) lofi.step_by_noteid[i]=lofi.step_by_noteid[i-12]<<1;
}

/* Init.
 */

int lofi_init(int rate,int chanc) {
  if (lofi.rate) return -1;
  if ((rate<LOFI_RATE_MIN)||(rate>LOFI_RATE_MAX)) return -1;
  if ((chanc<LOFI_CHANC_MIN)||(chanc>LOFI_CHANC_MAX)) return -1;
  lofi.rate=rate;
  lofi.chanc=chanc;
  lofi_calculate_steps();
  return 0;
}

/* Called only when song present and no delay.
 * Process any immediate events and finish with lofi.songdelay>0.
 */
 
static void lofi_update_song() {
  for (;;) {
    if ((lofi.songp>=lofi.songc)||!lofi.song[lofi.songp]) {
      // Repeat. Force a trivial delay, in case the song didn't contain any.
      lofi.songdelay=1;
      lofi.songp=0;
      return;
    }
    uint8_t lead=lofi.song[lofi.songp++];
    if (!(lead&0x80)) {
      lofi.songdelay=(lead*lofi.rate)/1000;
      return;
    }
    // 1cccnnnn nnnddddd dddddddd : NOTE. (c) channel id, (n) note id, (d) sustain time ms.
    if (lead>lofi.songc-2) {
      lofi.song=0;
      return;
    }
    int chid=(lead>>4)&7;
    int noteid=((lead<<3)&0x78)|(lofi.song[lofi.songp]>>5);
    int durms=((lofi.song[lofi.songp]&0x1f)<<8)|lofi.song[lofi.songp+1];
    lofi.songp+=2;
    lofi_note(lofi.channelv[chid].program,lofi.channelv[chid].trim,noteid,noteid,durms);
  }
}

/* Update, single output channel, any length.
 */
 
static void lofi_update_mono(int16_t *v,int c) {
  while (c>0) {
    int updc=c;
  
    if (lofi.song) {
      if (lofi.songdelay<=0) {
        lofi_update_song();
      }
      if (lofi.song&&(lofi.songdelay>0)) {
        updc=lofi.songdelay;
        if (updc>c) updc=c;
        lofi.songdelay-=updc;
      }
    }
    
    struct lofi_voice *voice=lofi.voicev;
    int i=lofi.voicec;
    for (;i-->0;voice++) {
      lofi_voice_update(v,updc,voice);
    }
    
    v+=updc;
    c-=updc;
  }
}

/* Update.
 */

void lofi_update(int16_t *v,int c) {
  memset(v,0,c<<1);
  if (lofi.chanc>1) {
    // We are strictly mono internally. We'll output multiple channels, but only by running mono first, then duplicating.
    if (c%lofi.chanc) return;
    int framec=c/lofi.chanc;
    lofi_update_mono(v,framec);
    int16_t *dst=v+c;
    const int16_t *src=v+framec;
    while (framec-->0) {
      src--;
      int i=lofi.chanc;
      while (i-->0) {
        dst--;
        *dst=*src;
      }
    }
  } else {
    lofi_update_mono(v,c);
  }
  while (lofi.voicec&&!lofi.voicev[lofi.voicec-1].ttl) lofi.voicec--;
}

/* Start song.
 */
 
void lofi_play_song(const void *src,int srcc) {
  lofi.song=0;
  if (!src||!srcc) return;
  if ((srcc<=20)||memcmp(src,"L\0\0\xf1",4)) return; // 20 = 4 signature + 2 * 8 channel headers. Sic '<=', require at least one event.
  const uint8_t *SRC=src;
  memcpy(lofi.channelv,SRC+4,16); // All fields 8-bit, just shovel them in.
  lofi.song=SRC+20;
  lofi.songc=srcc-20;
  lofi.songp=0;
  lofi.songdelay=0;
}
