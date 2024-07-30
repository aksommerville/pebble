#include "pblrt_internal.h"

/* Quit.
 */
 
void pblrt_drivers_quit() {
  pblrt_audio_play(0);
  pblrt_video_del(pblrt.video); pblrt.video=0;
  pblrt_audio_del(pblrt.audio); pblrt.audio=0;
  pblrt_input_del(pblrt.input); pblrt.input=0;
}

/* The generic initialization rigamarole.
 */
 
static void *pblrt_drivers_init_1(
  const char *intfname,
  const void *type_by_name,
  const void *type_by_index,
  void *instantiate,
  const char *request,
  const void *delegate,
  const void *setup
) {
  const void *(*TYPE_BY_NAME)(const char *name,int namec)=type_by_name;
  const void *(*TYPE_BY_INDEX)(int p)=type_by_index;
  void *(*INSTANTIATE)(const void *type,const void *delegate,const void *setup)=instantiate;
  if (request) {
    int srcp=0;
    while (request[srcp]) {
      if (request[srcp]==',') {
        srcp++;
        continue;
      }
      const char *token=request+srcp;
      int tokenc=0;
      while (request[srcp]&&(request[srcp++]!=',')) tokenc++;
      const void *type=TYPE_BY_NAME(token,tokenc);
      if (!type) {
        fprintf(stderr,"%s: Ignoring unknown %s driver '%.*s'\n",pblrt.exename,intfname,tokenc,token);
        continue;
      }
      void *instance=INSTANTIATE(type,delegate,setup);
      if (instance) return instance;
    }
  } else {
    int p=0;
    for (;;p++) {
      const struct pblrt_video_type *type=TYPE_BY_INDEX(p);
      if (!type) return 0;
      if (type->appointment_only) continue;
      void *instance=INSTANTIATE(type,delegate,setup);
      if (instance) return instance;
    }
  }
  return 0;
}

/* Populate (fbw,fbh,title,iconrgba,iconw,iconh) if possible, from ROM.
 */
 
static int pblrt_drivers_gather_video_setup(struct pblrt_video_setup *setup) {

  // "title" => Window title. Optional, use romname if absent.
  const char *title=0;
  int titlec=pblrt_rom_get_metadata_text(&title,"title",5);
  if (titlec>0) {
    while (titlec&&((unsigned char)title[titlec-1]<=0x20)) titlec--;
    while (titlec&&((unsigned char)title[0]<=0x20)) { titlec--; title++; }
    if ((titlec>0)&&(titlec<64)) {
      char *ztitle=malloc(titlec+1);
      if (ztitle) {
        memcpy(ztitle,title,titlec);
        ztitle[titlec]=0;
        int i=titlec; while (i-->0) {
          if ((unsigned char)ztitle[i]<0x20) ztitle[i]=' ';
        }
        pblrt.title_storage=ztitle;
        setup->title=ztitle;
      }
    }
  }
  if (!setup->title) setup->title=pblrt.romname;
  
  // "fb". Required.
  const char *src;
  int srcc=pblrt_rom_get_metadata(&src,"fb",2);
  setup->fbw=setup->fbh=0;
  int srcp=0;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    setup->fbw*=10;
    setup->fbw+=src[srcp++]-'0';
    if (setup->fbw>PBL_FRAMEBUFFER_LIMIT) { setup->fbw=0; break; }
  }
  if ((srcp<srcc)&&(src[srcp++]=='x')) {
    while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
      setup->fbh*=10;
      setup->fbh+=src[srcp++]-'0';
      if (setup->fbh>PBL_FRAMEBUFFER_LIMIT) { setup->fbh=0; break; }
    }
  }
  if ((setup->fbw<1)||(setup->fbh<1)) {
    fprintf(stderr,"%s: Invalid framebuffer size '%.*s'. Must be in 1x1..1024x1024.\n",pblrt.romname,srcc,src);
    return -2;
  }
  pblrt.fbw=setup->fbw;
  pblrt.fbh=setup->fbh;
  
  // "iconImage#" => Window icon. Optional.
  if ((srcc=pblrt_rom_get_metadata(&src,"iconImage#",10))>0) {
    int rid=0;
    if ((sr_int_eval(&rid,src,srcc)>=2)&&(rid>0)&&(rid<=0xffff)) {
      const void *serial=0;
      int serialc=pblrt_rom_get(&serial,PBL_TID_image,rid);
      if (serialc>0) {
        fprintf(stderr,"%s:%d:TODO: Load icon from %d bytes serial (image:%.*s)\n",__FILE__,__LINE__,serialc,srcc,src);//TODO
      }
    }
  }
  
  return 0;
}

/* Init video.
 */
 
static int pblrt_drivers_init_video() {
  int err;
  struct pblrt_video_delegate delegate={
    .cb_close=pblrt_cb_close,
    .cb_focus=pblrt_cb_focus,
    .cb_key=pblrt_cb_key,
  };
  struct pblrt_video_setup setup={
    .device=pblrt.video_device,
    .w=pblrt.video_w,
    .h=pblrt.video_h,
    .fullscreen=pblrt.fullscreen,
  };
  if ((err=pblrt_drivers_gather_video_setup(&setup))<0) return err;
  if (!(pblrt.video=pblrt_drivers_init_1("video",pblrt_video_type_by_name,pblrt_video_type_by_index,pblrt_video_new,pblrt.video_driver,&delegate,&setup))) {
    fprintf(stderr,"%s: Failed to initialize any video driver.\n",pblrt.exename);
    return -2;
  }
  fprintf(stderr,"%s: Using video driver '%s'\n",pblrt.exename,pblrt.video->type->name);
  return 0;
}

/* Determine length and allocate internal audio buffer.
 * Call only if (pblrt.audio) was successfully created.
 */
 
static int pblrt_drivers_prep_audio_buffer() {
  double rate=pblrt.audio->rate;
  double vframe=1.0/60.0;
  double buflens=vframe*2.0; // I think 2 video frames should be safe, and hopefully that's not too much latency.
  int framec=(int)(rate*buflens);
  if (framec<128) framec=128; // Sanity limit.
  int samplec=framec*pblrt.audio->chanc;
  if (!(pblrt.abuf=calloc(sizeof(int16_t),samplec))) return -1;
  pblrt.abufa=samplec;
  pblrt.synth_limit=samplec;
  return 0;
}

/* Init audio.
 */
 
static int pblrt_drivers_init_audio() {
  struct pblrt_audio_delegate delegate={
    .cb_pcm=pblrt_cb_pcm,
  };
  struct pblrt_audio_setup setup={
    .device=pblrt.audio_device,
    .rate=pblrt.audio_rate,
    .chanc=pblrt.audio_chanc,
    .buffer=pblrt.audio_buffer,
  };
  if (!(pblrt.audio=pblrt_drivers_init_1("audio",pblrt_audio_type_by_name,pblrt_audio_type_by_index,pblrt_audio_new,pblrt.audio_driver,&delegate,&setup))) {
    fprintf(stderr,"%s: Failed to initialize any audio driver. Proceeding without audio.\n",pblrt.exename);
  } else {
    fprintf(stderr,
      "%s: Using audio driver '%s', rate=%d, chanc=%d\n",
      pblrt.exename,pblrt.audio->type->name,pblrt.audio->rate,pblrt.audio->chanc
    );
    return pblrt_drivers_prep_audio_buffer();
  }
  return 0;
}

/* Init input.
 */
 
static int pblrt_drivers_init_input() {
  struct pblrt_input_delegate delegate={
    .cb_connect=pblrt_cb_connect,
    .cb_disconnect=pblrt_cb_disconnect,
    .cb_button=pblrt_cb_button,
  };
  struct pblrt_input_setup setup={
    .path=0,
  };
  if (!(pblrt.input=pblrt_drivers_init_1("input",pblrt_input_type_by_name,pblrt_input_type_by_index,pblrt_input_new,pblrt.input_driver,&delegate,&setup))) {
    if (pblrt.video&&pblrt.video->type->provides_keyboard) {
      fprintf(stderr,"%s: Failed to initialize any input driver. Proceeding with system keyboard only.\n",pblrt.exename);
    } else {
      fprintf(stderr,"%s: Failed to initialize any input driver.\n",pblrt.exename);
      return -2;
    }
  } else {
    fprintf(stderr,"%s: Using input driver '%s'\n",pblrt.exename,pblrt.input->type->name);
  }
  return 0;
}
 
/* Init.
 */ 

int pblrt_drivers_init() {
  int err;
  if ((err=pblrt_drivers_init_video())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error standing video driver.\n",pblrt.exename);
    return -2;
  }
  if ((err=pblrt_drivers_init_audio())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error standing audio driver.\n",pblrt.exename);
    return -2;
  }
  if ((err=pblrt_drivers_init_input())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error standing input driver.\n",pblrt.exename);
    return -2;
  }
  return 0;
}

/* Update.
 */

int pblrt_drivers_update() {
  int err;
  if (pblrt.video&&pblrt.video->type->update&&((err=pblrt.video->type->update(pblrt.video))<0)) return err;
  if (pblrt.audio&&pblrt.audio->type->update&&((err=pblrt.audio->type->update(pblrt.audio))<0)) return err;
  if (pblrt.input&&pblrt.input->type->update&&((err=pblrt.input->type->update(pblrt.input))<0)) return err;
  return 0;
}

/* Convenient aliases to driver calls.
 */

void pblrt_audio_play(int play) {
  if (!pblrt.audio||!pblrt.audio->type->play) return;
  if (play) {
    if (pblrt.audio->playing) return;
  } else {
    if (!pblrt.audio->playing) return;
  }
  pblrt.audio->type->play(pblrt.audio,play);
}

int pblrt_audio_lock() {
  if (!pblrt.audio||!pblrt.audio->type->lock) return 0;
  return pblrt.audio->type->lock(pblrt.audio);
}

void pblrt_audio_unlock() {
  if (!pblrt.audio||!pblrt.audio->type->unlock) return;
  pblrt.audio->type->unlock(pblrt.audio);
}

/* Create and delete drivers.
 */
 
struct pblrt_video *pblrt_video_new(const struct pblrt_video_type *type,const struct pblrt_video_delegate *delegate,const struct pblrt_video_setup *setup) {
  if (!type||!delegate||!setup) return 0;
  struct pblrt_video *video=calloc(1,type->objlen);
  if (!video) return 0;
  video->type=type;
  video->delegate=*delegate;
  if (type->init(video,setup)<0) {
    pblrt_video_del(video);
    return 0;
  }
  return video;
}

struct pblrt_audio *pblrt_audio_new(const struct pblrt_audio_type *type,const struct pblrt_audio_delegate *delegate,const struct pblrt_audio_setup *setup) {
  if (!type||!delegate||!setup) return 0;
  struct pblrt_audio *audio=calloc(1,type->objlen);
  if (!audio) return 0;
  audio->type=type;
  audio->delegate=*delegate;
  if (type->init(audio,setup)<0) {
    pblrt_audio_del(audio);
    return 0;
  }
  return audio;
}

struct pblrt_input *pblrt_input_new(const struct pblrt_input_type *type,const struct pblrt_input_delegate *delegate,const struct pblrt_input_setup *setup) {
  if (!type||!delegate||!setup) return 0;
  struct pblrt_input *input=calloc(1,type->objlen);
  if (!input) return 0;
  input->type=type;
  input->delegate=*delegate;
  if (type->init(input,setup)<0) {
    pblrt_input_del(input);
    return 0;
  }
  return input;
}

void pblrt_video_del(struct pblrt_video *video) {
  if (!video) return;
  if (video->type->del) video->type->del(video);
  free(video);
}

void pblrt_audio_del(struct pblrt_audio *audio) {
  if (!audio) return;
  if (audio->type->del) audio->type->del(audio);
  free(audio);
}

void pblrt_input_del(struct pblrt_input *input) {
  if (!input) return;
  if (input->type->del) input->type->del(input);
  free(input);
}

/* Language changed.
 */
 
void pblrt_lang_changed() {
  //TODO Amend video driver interface to allow changing title after construction.
  //TODO Any other global state that depends on language?
}
