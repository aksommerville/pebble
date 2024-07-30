#include "pbltool_internal.h"

struct pbltool pbltool={0};

/* Main.
 */
 
int main(int argc,char **argv) {
  int err;
  
  if ((err=pbltool_configure(argc,argv))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error configuring.\n",pbltool.exename);
    return 1;
  }
  
  if (!pbltool.command) {
    pbltool_print_usage(0);
    return 1;
  }
  // If the command is "help", pbltool_configure already printed it.
  if (!strcmp(pbltool.command,"help")) return 0;
  
  #define _(tag) if (!strcmp(pbltool.command,#tag)) err=pbltool_main_##tag();
       _(pack)
  else _(unpack)
  else _(bundle)
  else _(unbundle)
  else _(list)
  else {
    fprintf(stderr,"%s: Unknown command '%s'. Try '%s --help'\n",pbltool.exename,pbltool.command,pbltool.exename);
    return 1;
  }
  #undef _
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error executing command '%s'\n",pbltool.exename,pbltool.command);
    return 1;
  }
  return 0;
}
