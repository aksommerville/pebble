#include "pbltool_internal.h"
#include "opt/midi/midi.h"

/* lofi from midi, in context.
 */
 
static int pbltool_lofi_from_midi_inner(struct sr_encoder *dst,struct midi_file *src,struct pbltool_res *res) {
  #define HOLD_LIMIT 32
  struct hold {
    uint8_t chid;
    uint8_t noteid;
    int when;
    int dstp; // Start of 3-byte event.
  } holdv[HOLD_LIMIT];
  int holdc=0;
  if (sr_encode_raw(dst,"L\0\0\xf1",4)<0) return -1;
  int chhdrp=dst->c;
  if (sr_encode_raw(dst,"\0\x80\0\x80\0\x80\0\x80\0\x80\0\x80\0\x80\0\x80",16)<0) return -1;
  int now=0;
  for (;;) {
    struct midi_event event={0};
    int delay=midi_file_next(&event,src);
    if (delay<0) {
      if (midi_file_is_finished(src)) break;
      fprintf(stderr,"%s: Error streaming from MIDI file\n",res->path);
      return -2;
    }
    if (delay) {
      now+=delay;
      if (midi_file_advance(src,delay)<0) return -1;
      if (delay>=10000) {
        fprintf(stderr,"%s:ERROR: Panic due to unreasonable %d ms delay\n",res->path,delay);
        return -2;
      }
      while (delay>=0x7f) {
        if (sr_encode_u8(dst,0x7f)<0) return -1;
        delay-=0x7f;
      }
      if (delay>0) {
        if (sr_encode_u8(dst,delay)<0) return -1;
      }
    } else switch (event.opcode) {
    
      case 0x80: {
          int i=holdc; while (i-->0) {
            struct hold *hold=holdv+i;
            if ((hold->chid==event.chid)&&(hold->noteid==event.a)) {
              int dur=now-hold->when;
              if (dur>0x1fff) {
                fprintf(stderr,
                  "%s:WARNING: Truncating note 0x%02x on channel %d at time %d ms from %d ms to %d ms\n",
                  res->path,hold->noteid,hold->chid,hold->when,dur,0x1fff
                );
                dur=0x1fff;
              }
              uint8_t *evt=((uint8_t*)(dst->v))+hold->dstp;
              evt[1]|=dur>>8;
              evt[2]=dur;
              holdc--;
              memmove(hold,hold+1,sizeof(struct hold)*(holdc-i));
              break;
            }
          }
        } break;
        
      case 0x90: {
          if (event.chid>=0x08) {
            fprintf(stderr,"%s:WARNING: Ignoring event on channel %d; we only go thru 7.\n",res->path,event.chid);
            continue;
          }
          if (holdc>=HOLD_LIMIT) {
            fprintf(stderr,
              "%s:WARNING: Exceeded held-note limit of %d. Note 0x%02x on channel %d at time %d ms will get zero sustain.\n",
              res->path,HOLD_LIMIT,event.a,event.chid,now
            );
          } else {
            holdv[holdc].chid=event.chid;
            holdv[holdc].noteid=event.a;
            holdv[holdc].when=now;
            holdv[holdc].dstp=dst->c;
            holdc++;
          }
          if (sr_encode_u8(dst,0x80|(event.chid<<4)|(event.a>>3))<0) return -1;
          if (sr_encode_u8(dst,(event.a<<5))<0) return -1;
          if (sr_encode_u8(dst,0)<0) return -1;
        } break;
        
      case 0xb0: if (event.chid<8) switch (event.a) {
          case 0x20: {
              uint8_t *pid=((uint8_t*)(dst->v))+chhdrp+(event.chid*2);
              if (event.b&1) (*pid)|=0x80;
              else (*pid)&=~0x80;
            } break;
          case 0x07: ((uint8_t*)(dst->v))[chhdrp+(event.chid*2)+1]=event.b<<1; break;
        } break;
        
      case 0xc0: if (event.chid<8) {
          uint8_t *pid=((uint8_t*)(dst->v))+chhdrp+(event.chid*2);
          (*pid)=((*pid)&0x80)|(event.a&0x7f);
        } break;
        
    }
  }
  if (holdc) {
    fprintf(stderr,"%s:WARNING: Song ended with %d notes still down; they will be written as sustain zero.\n",res->path,holdc);
  }
  #undef HOLD_LIMIT
  return 0;
}

/* lofi from midi, main entry point.
 */
 
static int pbltool_lofi_from_midi(struct pbltool_res *res) {
  fprintf(stderr,"%s... srcc=%d\n",__func__,res->serialc);
  // Instantiating with a rate of 1000 means it will naturally express in milliseconds, as we need.
  struct midi_file *src=midi_file_new(res->serial,res->serialc,1000);
  if (!src) {
    fprintf(stderr,"%s: Failed to decode MIDI file\n",res->path);
    return -2;
  }
  struct sr_encoder dst={0};
  int err=pbltool_lofi_from_midi_inner(&dst,src,res);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error converting MIDI to Lofi Song\n",res->path);
    midi_file_del(src);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  midi_file_del(src);
  pbltool_res_handoff_serial(res,dst.v,dst.c);
  return 0;
}

/* midi from lofi, main entry point.
 */
 
static int pbltool_midi_from_lofi(struct pbltool_res *res) {
  fprintf(stderr,"%s:%d:TODO: %s\n",__FILE__,__LINE__,__func__);
  return 0;
}

/* Entry points.
 */
 
int pbltool_compile_song(struct pbltool_res *res) {
  if ((res->serialc>=4)&&!memcmp(res->serial,"MThd",4)) return pbltool_lofi_from_midi(res);
  return 0;
}

int pbltool_uncompile_song(struct pbltool_res *res) {
  if ((res->serialc>=4)&&!memcmp(res->serial,"L\0\0\xf1",4)) return pbltool_midi_from_lofi(res);
  return 0;
}
