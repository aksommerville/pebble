#include "evdev_internal.h"

/* Delete.
 */

static void evdev_del(struct pblrt_input *driver) {
  if (DRIVER->devicev) {
    while (DRIVER->devicec-->0) evdev_device_del(DRIVER->devicev[DRIVER->devicec]);
    free(DRIVER->devicev);
  }
  if (DRIVER->inofd>=0) close(DRIVER->inofd);
  if (DRIVER->path) free(DRIVER->path);
  if (DRIVER->pollfdv) free(DRIVER->pollfdv);
}

/* Init.
 */

static int evdev_init(struct pblrt_input *driver,const struct pblrt_input_setup *setup) {
  const char *path=setup->path;
  if (!path||!path[0]) path="/dev/input";
  
  DRIVER->inofd=-1;
  DRIVER->rescan=1;
  DRIVER->devid_next=1;
  
  if (!(DRIVER->path=strdup(path))) return -1;
  if ((DRIVER->inofd=inotify_init())>=0) {
    inotify_add_watch(DRIVER->inofd,DRIVER->path,IN_ATTRIB|IN_CREATE|IN_MOVED_TO);
  }
  
  return 0;
}

/* Access to device list.
 */
 
struct evdev_device *evdev_device_by_index(const struct pblrt_input *driver,int p) {
  if (p<0) return 0;
  if (p>=DRIVER->devicec) return 0;
  return DRIVER->devicev[p];
}

struct evdev_device *evdev_device_by_devid(const struct pblrt_input *driver,int devid) {
  struct evdev_device **p=DRIVER->devicev;
  int i=DRIVER->devicec;
  for (;i-->0;p++) if ((*p)->devid==devid) return *p;
  return 0;
}

struct evdev_device *evdev_device_by_fd(const struct pblrt_input *driver,int fd) {
  struct evdev_device **p=DRIVER->devicev;
  int i=DRIVER->devicec;
  for (;i-->0;p++) if ((*p)->fd==fd) return *p;
  return 0;
}

struct evdev_device *evdev_device_by_kid(const struct pblrt_input *driver,int kid) {
  struct evdev_device **p=DRIVER->devicev;
  int i=DRIVER->devicec;
  for (;i-->0;p++) if ((*p)->kid==kid) return *p;
  return 0;
}

static int evdev_devid_by_index(struct pblrt_input *driver,int p) {
  if (p<0) return 0;
  if (p>=DRIVER->devicec) return 0;
  return DRIVER->devicev[p]->devid;
}

static const char *evdev_ids_by_devid(int *vid,int *pid,int *version,struct pblrt_input *driver,int devid) {
  struct evdev_device *device=evdev_device_by_devid(driver,devid);
  if (!device) return 0;
  *vid=device->vid;
  *pid=device->pid;
  *version=device->version;
  return device->name;
}

static int evdev_list_buttons(
  struct pblrt_input *driver,
  int devid,
  int (*cb)(int btnid,int pblbtnid,int hidusage,int lo,int hi,int rest,void *userdata),
  void *userdata
) {
  struct evdev_device *device=evdev_device_by_devid(driver,devid);
  if (!device) return 0;
  return evdev_device_for_each_button(device,cb,userdata);
}

/* Add device (privateish).
 */
 
int evdev_add_device(struct pblrt_input *driver,struct evdev_device *device) {
  if (!device) return -1;
  if (DRIVER->devicec>=DRIVER->devicea) {
    int na=DRIVER->devicea+16;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(DRIVER->devicev,sizeof(void*)*na);
    if (!nv) return -1;
    DRIVER->devicev=nv;
    DRIVER->devicea=na;
  }
  DRIVER->devicev[DRIVER->devicec++]=device;
  return 0;
}

/* Disconnect device.
 */
 
void evdev_device_disconnect(struct pblrt_input *driver,struct evdev_device *device) {
  if (!device) return;
  int p=0; for (;p<DRIVER->devicec;p++) {
    if (DRIVER->devicev[p]==device) {
      DRIVER->devicec--;
      memmove(DRIVER->devicev+p,DRIVER->devicev+p+1,sizeof(void*)*(DRIVER->devicec-p));
      evdev_device_del(device);
      return;
    }
  }
}

/* Type definition.
 */
 
const struct pblrt_input_type pblrt_input_type_evdev={
  .name="evdev",
  .desc="Generic input devices for Linux.",
  .objlen=sizeof(struct pblrt_input_evdev),
  .del=evdev_del,
  .init=evdev_init,
  .update=evdev_update,
  .devid_by_index=evdev_devid_by_index,
  .ids_by_devid=evdev_ids_by_devid,
  .list_buttons=evdev_list_buttons,
};
