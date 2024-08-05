/* pblrt_romsrc.c
 * This is the only file that knows at runtime whether we have an embedded ROM file.
 * It gets compiled two different ways: Embedded and External.
 */

#include "pblrt_internal.h"
 
#define EMBEDDED 1
#define EXTERNAL 2
#if ROMSRC==EMBEDDED
  extern const unsigned char pbl_embedded_rom[];
  extern const int pbl_embedded_rom_size;
  const int pblrt_romsrc_uses_external_rom=0;
#elif ROMSRC==EXTERNAL
  const int pblrt_romsrc_uses_external_rom=1;
#else
  #error "Please compile with either -DROMSRC=EMBEDDED or -DROMSRC=EXTERNAL."
#endif

/* Quit.
 */
 
void pblrt_romsrc_quit() {
}

/* Init.
 */
 
int pblrt_romsrc_init() {
  #if ROMSRC==EMBEDDED
    pblrt.romname=pblrt.exename;
    pblrt.rom=pbl_embedded_rom;
    pblrt.romc=pbl_embedded_rom_size;
    pblrt.ownrom=0;
    if (pblrt.romc<1) return -1;
  #else
    if (!pblrt.rompath) {
      fprintf(stderr,"%s: ROM file required.\n",pblrt.exename);
      return -2;
    }
    pblrt.romname=pblrt.rompath;
    if ((pblrt.romc=file_read(&pblrt.rom,pblrt.rompath))<0) {
      fprintf(stderr,"%s: Failed to read file.\n",pblrt.rompath);
      return -2;
    }
    pblrt.ownrom=1;
  #endif
  return 0;
}
