#include "pblrt_internal.h"
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#define PBLRT_FRAME_TIME     (1.0/60.0) /* TODO Configurable? */
#define PBLRT_FRAME_TIME_MAX (1.0/40.0) /* Reported interval clamps hard to this. */

/* Primitives.
 */
 
static double pblrt_now_real() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (double)tv.tv_sec+(double)tv.tv_usec/1000000.0;
}

static double pblrt_now_cpu() {
  struct timespec tv={0};
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&tv);
  return (double)tv.tv_sec+(double)tv.tv_nsec/1000000000.0;
}

static void pblrt_sleep(double s) {
  usleep((int)(s*1000000.0));
}

/* Init.
 */
 
void pblrt_clock_init() {
  // Fudge the "last observed time" backward by one frame, so we don't sleep on the very first update.
  pblrt.starttime_real=pblrt_now_real();
  pblrt.clock_recent=pblrt.starttime_real-PBLRT_FRAME_TIME;
  pblrt.starttime_cpu=pblrt_now_cpu();
  pblrt.framec=0;
  pblrt.clock_faultc=0;
}

/* Update.
 */
 
double pblrt_clock_update() {
  double now=pblrt_now_real();
  double elapsed=now-pblrt.clock_recent;
  if (elapsed<0.0) {
    pblrt.clock_faultc++;
    elapsed=PBLRT_FRAME_TIME;
  } else if (elapsed>PBLRT_FRAME_TIME_MAX) {
    elapsed=PBLRT_FRAME_TIME_MAX;
  } else if (elapsed<PBLRT_FRAME_TIME) {
    for (;;) {
      pblrt_sleep(PBLRT_FRAME_TIME-elapsed+0.0001); // +100us to avoid coming up just a little short
      now=pblrt_now_real();
      elapsed=now-pblrt.clock_recent;
      if (elapsed>=PBLRT_FRAME_TIME) break;
    }
  }
  pblrt.clock_recent=now;
  pblrt.framec++;
  return elapsed;
}

/* Generate report.
 */
 
int pblrt_clock_report(double *rate,double *cpu) {
  if (pblrt.framec<1) return 0;
  double elapsed_real=pblrt_now_real()-pblrt.starttime_real;
  double elapsed_cpu=pblrt_now_cpu()-pblrt.starttime_cpu;
  *rate=(double)pblrt.framec/elapsed_real;
  *cpu=elapsed_cpu/elapsed_real;
  return pblrt.framec;
}
