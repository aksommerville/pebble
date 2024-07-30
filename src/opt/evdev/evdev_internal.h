#ifndef EVDEV_INTERNAL_H
#define EVDEV_INTERNAL_H

#include "pblrt/pblrt_drivers.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/inotify.h>

struct pblrt_input_evdev {
  struct pblrt_input hdr;
  struct evdev_device **devicev;
  int devicec,devicea;
  char *path;
  int pathc;
  int inofd;
  struct pollfd *pollfdv;
  int pollfda;
  int rescan;
  int devid_next; // When it overflows, we can't add more devices. Would take billions of reconnections.
};

#define DRIVER ((struct pblrt_input_evdev*)driver)

struct evdev_device {
  int fd;
  int devid;
  int kid;
  char *name;
  int vid,pid,version;
};

int evdev_add_device(struct pblrt_input *driver,struct evdev_device *device);
void evdev_device_del(struct evdev_device *device);
struct evdev_device *evdev_device_new();
int evdev_device_acquire_ids(struct evdev_device *device);

// Manages disconnect and removal on errors. <0 returned here is a serious fatal error.
int evdev_device_update(struct pblrt_input *driver,struct evdev_device *device);

/* Button declarations are *not* cached; this performs some I/O.
 * Calls (cb) for each KEY, ABS, REL, and SW event that the device claims to support.
 * (btnid) is (type<<16)|code.
 */
int evdev_device_for_each_button(
  struct evdev_device *device,
  int (*cb)(int btnid,int pblbtnid,int hidusage,int lo,int hi,int value,void *userdata),
  void *userdata
);

/* Close the underlying file and remove this device from our list.
 * Wise to do this when we find a device you know you don't need.
 * Some systems report a bunch of like front panel switches or HDMI control channels or other weirdness.
 */
void evdev_device_disconnect(struct pblrt_input *driver,struct evdev_device *device);

int evdev_update(struct pblrt_input *driver);

int evdev_guess_pblbtnid(int type,int code);
int evdev_guess_hidusage(int type,int code);

struct evdev_device *evdev_device_by_index(const struct pblrt_input *driver,int p);
struct evdev_device *evdev_device_by_devid(const struct pblrt_input *driver,int devid);
struct evdev_device *evdev_device_by_fd(const struct pblrt_input *driver,int fd);
struct evdev_device *evdev_device_by_kid(const struct pblrt_input *driver,int kid);

#endif
