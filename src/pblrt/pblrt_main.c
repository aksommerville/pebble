#include "pblrt_internal.h"
#include <signal.h>

struct pblrt pblrt={0};

/* Signal handler.
 */
 
static void pblrt_rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++(pblrt.terminate)>=3) {
        fprintf(stderr,"%s: Too many unprocessed signals.\n",pblrt.exename);
        exit(1);
      } break;
  }
}

/* Final report.
 */
 
static void pblrt_final_report() {
  if (pblrt.termstatus) {
    fprintf(stderr,"%s: Abnormal termination.\n",pblrt.romname);
  } else {
    double rate,cpu;
    int framec=pblrt_clock_report(&rate,&cpu);
    if (framec>0) {
      fprintf(stderr,"%s: %d frames, average rate %.03f Hz. CPU load %.06f.\n",pblrt.romname,framec,rate,cpu);
    } else {
      fprintf(stderr,"%s: Normal termination and nothing to report.\n",pblrt.romname);
    }
  }
}

/* Render frame.
 * This interacts with both exec and the video driver.
 */
 
static int pblrt_render() {
  void *fb=0;
  int err=pblrt_exec_call_render(&fb);
  if (err<0) return err;
  if (fb) {
    err=pblrt.video->type->commit_framebuffer(pblrt.video,fb,pblrt.fbw,pblrt.fbh);
  } else if (pblrt.video->type->skip) {
    pblrt.video->type->skip(pblrt.video);
  }
  return err;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  int err,status=0;
  
  signal(SIGINT,pblrt_rcvsig);
  signal(SIGUSR1,pblrt_rcvsig);
  
  if ((err=pblrt_configure(argc,argv))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error configuring.\n",pblrt.exename);
    return 1;
  }
  if (pblrt.terminate) return 0; // eg --help
  
  if ((err=pblrt_romsrc_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error acquiring ROM.\n",pblrt.exename);
    return 1;
  }
  
  if (!pblrt.lang) pblrt.lang=pblrt_choose_default_language();
  
  pblrt_load_store();
  
  if ((err=pblrt_inmgr_init(&pblrt.inmgr))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error loading input configuration.\n",pblrt.exename);
    return 1;
  }
  
  if ((err=pblrt_exec_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error initializing execution core.\n",pblrt.exename);
    return 1;
  }
  
  if ((err=pblrt_drivers_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error initializing drivers.\n",pblrt.exename);
    return 1;
  }
  
  if ((err=pblrt_exec_call_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error initializing game.\n",pblrt.romname);
    return 1;
  }
  
  pblrt_audio_play(1);
  pblrt_clock_init();
  
  while (!pblrt.terminate) {
    double elapsed=pblrt_clock_update();
    if ((err=pblrt_drivers_update())<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error updating drivers.\n",pblrt.exename);
      pblrt.termstatus=1;
      break;
    }
    if ((err=pblrt_inmgr_update(&pblrt.inmgr))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error updating input manager.\n",pblrt.exename);
      pblrt.termstatus=1;
      break;
    }
    if (pblrt.hardpause) continue;
    if ((err=pblrt_exec_update(elapsed))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error updating game.\n",pblrt.romname);
      pblrt.termstatus=1;
      break;
    }
    if ((err=pblrt_render())<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error rendering frame.\n",pblrt.romname);
      pblrt.termstatus=1;
      break;
    }
    if ((err=pblrt_exec_refill_audio())<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error collecting audio.\n",pblrt.romname);
      pblrt.termstatus=1;
      break;
    }
  }
  
  pblrt_audio_play(0);
  pblrt_exec_call_quit();
  pblrt_final_report();
  pblrt_drivers_quit();
  pblrt_exec_quit();
  pblrt_romsrc_quit();
  return pblrt.termstatus;
}
