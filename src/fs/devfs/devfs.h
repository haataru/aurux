#ifndef DEVFS_H
#define DEVFS_H

#include "../fs.h"

extern struct fs_driver devfs_driver;

void devfs_init(void);

#endif
