/*
  Support posix-like ioctl calls over a RT-Linux FIFO from
  user space to an RL-linux module.

  Original Author: Gordon Maclean

  Copyright by the National Center for Atmospheric Research 2004

*/

#ifndef IOCTL_FIFO_H
#define IOCTL_FIFO_H

#include <linux/ioctl.h>

/* symbols used by user and kernel space code. */

#define IOCTL_FIFO_MAGIC 'F'

#define GET_NUM_PORTS  _IOR(IOCTL_FIFO_MAGIC,0,int)


#ifdef __RTCORE_KERNEL__

/* Below here are symbols used by the ioctl_fifo module. */

#define __RTCORE_POLLUTED_APP__
#include <gpos_bridge/sys/gpos.h>
#include <rtl.h>
#include <rtl_pthread.h>
#include <rtl_fcntl.h>
#include <sys/rtl_types.h>
#include <sys/rtl_stat.h>

#include <linux/list.h>

struct ioctlCmd {
  int cmd;
  long size;
};

/* typedef for driver module function that is called to
 * satisfy an ioctl request.
 */
typedef int ioctlCallback_t(int cmd,int board, int port,
	void* buf, rtl_size_t len);


/* header that is sent over the FIFOs before the ioctl data */
struct ioctlHeader {
  int cmd;		/* ioctl command */
  int port;		/* destination port */
  long size;		/* length in bytes of the data */
};

struct ioctlHandle {

  ioctlCallback_t *ioctlCallback;
  int boardNum;			/* which board number am I */
  struct ioctlCmd* ioctls;	/* pointer to array of struct ioctlCmds */
  int nioctls;			/* how many ioctlCmds do I support */

  char *inFifoName;
  char *outFifoName;
  int inFifofd;
  int outFifofd;
  int bytesRead;
  int bytesToRead;
  char  readETX;		/* logical: read EXT (003) character next */
  int icmd;
  struct ioctlHeader header;
  unsigned char *buf;
  long bufsize;

  rtl_pthread_mutex_t mutex;
  struct list_head list;

};

struct ioctlHandle*  openIoctlFIFO(const char* prefix,int boardNum,
	ioctlCallback_t* callback,int nioctls,struct ioctlCmd* ioctls);

void closeIoctlFIFO(struct ioctlHandle* ioctls);

char* makeDevName(const char* prefix, const char* suffix, int num);

#endif	/* __RTCORE_KERNEL__ */

#endif	/* IOCTL_FIFO_H */
