#ifndef PBLTOOL_INTERNAL_H
#define PBLTOOL_INTERNAL_H

#include "pebble/pebble.h"
#include "opt/fs/fs.h"
#include "opt/serial/serial.h"
#include "pbltool_rom.h"
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

extern struct pbltool {
  const char *exename;
  const char *command;
  const char *dstpath;
  const char **srcpathv;
  int srcpathc,srcpatha;
  int raw; // unpack
  int recompile; // bundle
} pbltool;

// Standard types only, or numeric.
int pbltool_tid_eval(const char *src,int srcc);
int pbltool_tid_repr(char *dst,int dsta,int tid);

/* If content needs to change, these will replace (res->serial).
 * Uncompilers may amend (name,comment,format) to influence the output file name.
 */
int pbltool_compile_metadata(struct pbltool_res *res);
int pbltool_compile_strings(struct pbltool_res *res);
int pbltool_compile_image(struct pbltool_res *res);
int pbltool_uncompile_metadata(struct pbltool_res *res);
int pbltool_uncompile_strings(struct pbltool_res *res);
int pbltool_uncompile_image(struct pbltool_res *res);

int pbltool_configure(int argc,char **argv);
void pbltool_print_usage(const char *topic);

int pbltool_main_pack();
int pbltool_main_unpack();
int pbltool_main_bundle();
int pbltool_main_unbundle();
int pbltool_main_list();

#endif
