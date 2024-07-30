#include "evdev_internal.h"

/* Check a file newly discovered in the devices directory.
 * Errors are for fatal context-wide problems only.
 * Not an evdev device, fails to open, etc, not an error.
 */
 
static int evdev_try_file(struct pblrt_input *driver,const char *base) {
  if (DRIVER->devid_next<1) return 0;
  
  /* Basename must be "eventN" where N is the decimal kid.
   * Already got it open? Done.
   */
  if (memcmp(base,"event",5)||!base[5]) return 0;
  int kid=0,basep=5;
  for (;base[basep];basep++) {
    int digit=base[basep]-'0';
    if ((digit<0)||(digit>9)) return 0;
    kid*=10;
    kid+=digit;
    if (kid>=999999) return 0;
  }
  if (evdev_device_by_kid(driver,kid)) return 0;
  
  /* On a typical desktop system, open will fail for most device. That's fine.
   * On consoles, it's more likely they will all open and we'll end up ignoring most of them. Also fine.
   */
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%s/%s",DRIVER->path,base);
  if ((pathc<=1)||(pathc>=sizeof(path))) return 0;
  int fd=open(path,O_RDONLY);
  if (fd<0) return 0;
  
  /* Provision the device object, hand off file to it, and attach it to the context eagerly.
   */
  struct evdev_device *device=evdev_device_new();
  if (!device) {
    close(fd);
    return -1;
  }
  device->fd=fd; // HANDOFF
  device->kid=kid;
  device->devid=DRIVER->devid_next++;
  if (evdev_add_device(driver,device)<0) {
    evdev_device_del(device);
    return -1;
  }
  
  /* Acquire IDs from kernel.
   * We could fail here if it's not a real evdev device, or for other reasons.
   */
  if (evdev_device_acquire_ids(device)<0) {
    evdev_device_disconnect(driver,device);
    return 0;
  }

  /* Grab. No worries if this fails, but it does matter sometimes.
   * eg on my Pi, if we don't grab the keyboard, it keeps talking to the console while we're running.
   */
  ioctl(device->fd,EVIOCGRAB,1);
  
  /* Alert our owner.
   * Beware that owner may disconnect the device during this callback, which will delete it for real.
   * So this must be the last step here. (which it should be anyway)
   */
  if (driver->delegate.cb_connect) {
    driver->delegate.cb_connect(device->devid);
  }
  return 0;
}

/* Scan directory.
 * This should only happen once, on the first update.
 * We could add a "poke" feature at any time, just set (DRIVER->rescan), and this runs again.
 */
 
static int evdev_scan(struct pblrt_input *driver) {
  DIR *dir=opendir(DRIVER->path);
  if (!dir) return -1;
  struct dirent *de;
  while (de=readdir(dir)) {
    if (de->d_type!=DT_CHR) continue;
    int err=evdev_try_file(driver,de->d_name);
    if (err<0) {
      closedir(dir);
      return err;
    }
  }
  closedir(dir);
  return 0;
}

/* Update inotify.
 * Handles closure on errors. Errors reported out of here are fatal.
 */
 
static int evdev_update_inotify(struct pblrt_input *driver) {
  char tmp[1024];
  int tmpc=read(DRIVER->inofd,tmp,sizeof(tmp));
  if (tmpc<=0) {
    close(DRIVER->inofd);
    DRIVER->inofd=-1;
    return 0;
  }
  int tmpp=0;
  while (tmpp<=tmpc-sizeof(struct inotify_event)) {
    struct inotify_event *event=(struct inotify_event*)(tmp+tmpp);
    tmpp+=sizeof(struct inotify_event);
    if (tmpp>tmpc-event->len) break;
    tmpp+=event->len;
    const char *base=event->name;
    int err=evdev_try_file(driver,base);
    if (err<0) return err;
  }
  return 0;
}

/* Iterate live files.
 */
 
int evdev_for_each_file(const struct pblrt_input *driver,int (*cb)(int fd,void *userdata),void *userdata) {
  int err;
  if (DRIVER->inofd>=0) {
    if (err=cb(DRIVER->inofd,userdata)) return err;
  }
  int p=0;
  for (;p<DRIVER->devicec;p++) {
    struct evdev_device *device=DRIVER->devicev[p];
    if (device->fd<0) continue;
    if (err=cb(device->fd,userdata)) return err;
  }
  return 0;
}

/* Update one file.
 */
 
int evdev_update_file(struct pblrt_input *driver,int fd) {
  if (fd<0) return 0;
  if (fd==DRIVER->inofd) return evdev_update_inotify(driver);
  struct evdev_device *device=evdev_device_by_fd(driver,fd);
  if (device) return evdev_device_update(driver,device);
  return 0;
}

/* Drop any device which has run out of funk.
 */
 
static void evdev_drop_defunct_devices(struct pblrt_input *driver) {
  int p=DRIVER->devicec;
  while (p-->0) {
    struct evdev_device *device=DRIVER->devicev[p];
    if (device->fd>=0) continue;
    DRIVER->devicec--;
    memmove(DRIVER->devicev+p,DRIVER->devicev+p+1,sizeof(void*)*(DRIVER->devicec-p));
    if (driver->delegate.cb_disconnect) driver->delegate.cb_disconnect(device->devid);
    evdev_device_del(device);
  }
}

/* Update, main.
 */
 
int evdev_update(struct pblrt_input *driver) {
  
  if (DRIVER->rescan) {
    DRIVER->rescan=0;
    if (evdev_scan(driver)<0) return -1;
  }
  evdev_drop_defunct_devices(driver);
  
  int pollfdc=DRIVER->devicec;
  if (DRIVER->inofd>=0) pollfdc++;
  if (pollfdc>DRIVER->pollfda) {
    int na=(pollfdc+16)&~15;
    if (na>INT_MAX/sizeof(struct pollfd)) return -1;
    void *nv=realloc(DRIVER->pollfdv,sizeof(struct pollfd)*na);
    if (!nv) return -1;
    DRIVER->pollfdv=nv;
    DRIVER->pollfda=na;
  }
  if (pollfdc<1) return 0;
  
  struct pollfd *p=DRIVER->pollfdv;
  if (DRIVER->inofd>=0) {
    p->fd=DRIVER->inofd;
    p->events=POLLIN|POLLERR|POLLHUP;
    p->revents=0;
    p++;
  }
  int i=DRIVER->devicec;
  struct evdev_device **device=DRIVER->devicev;
  for (;i-->0;p++,device++) {
    p->fd=(*device)->fd;
    p->events=POLLIN|POLLERR|POLLHUP;
    p->revents=0;
  }
  
  int err=poll(DRIVER->pollfdv,pollfdc,0);
  if (err<=0) return 0;
  
  for (p=DRIVER->pollfdv,i=pollfdc;i-->0;p++) {
    if (!p->revents) continue;
    if (evdev_update_file(driver,p->fd)<0) {
      fprintf(stderr,"evdev_update_file: error\n");
      return -1;
    }
  }
  
  return 0;
}
