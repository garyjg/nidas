/*
 * 	sample program to send SDLC data
 *
 * 	Sealevel and Seamac are registered trademarks of Sealevel Systems 
 * 	Incorporated.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the Lesser GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	3 of the License, or (at your option) any later version.
 * 	LGPL v3
 *
 *	(c) Copyright 2007-2013 Sealevel Systems Inc.
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <termios.h>
#include <unistd.h>

#include <linux/types.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>

#include "seamac.h"

int main(int argc, char* argv[])
{
	int fd, rc, i;
	int hdlc_disc = N_HDLC;
	struct seamac_params params;
	unsigned char *buf;
	int size = 256;
	char *devname = "/dev/ttySM0";

	if (argc > 1)
		devname = argv[1];
	if (argc > 2)
		size = atoi(argv[2]);
	printf("test: sending %u byte SDLC frames on %s\n", size, devname);
printf("test\n");
	buf = malloc(size);
printf("there's a buffer\n");
	if (!buf) {
		printf("can't allocate buffer\n");
		return ENOMEM;
	}
printf("allocated buffer\n");
	fd = open(devname, O_RDWR, 0);
printf("after open\n");
	if (fd < 0) {
		printf("open on device %s failed with err=%d %s\n",
			devname, errno, strerror(errno));
		return fd;
	}
printf("beforehdlc\n");
	// set N_HDLC line discipline (used for SDLC mode)
	rc = ioctl(fd, TIOCSETD, &hdlc_disc);
printf("hdlc worked\n");
	if(rc < 0) {
		printf("ERROR, can't set line discipline error=%d %s\n",
		       errno, strerror(errno));
		return rc;
	}
printf("set n_hdlc\n");

	// get current device parameters
	rc = ioctl(fd, SEAMAC_IOCTL_GPARAMS, &params);
printf("got current device parapms\n");
	if (rc < 0) {
		printf("ioctl(SEAMAC_IOCTL_GPARAMS) on device %s"
		       " failed with err=%d %s\n",
		       devname, errno, strerror(errno));
		return rc;
	}
printf("before params\n");
	// modify device parameters 
	params.mode = SEAMAC_MODE_SDLC;
	params.rate = 1000;    // 1kbps
	params.encoding = SEAMAC_ENCODE_NRZ;
	params.txclk = SEAMAC_CLK_BRG;
	params.rxclk = SEAMAC_CLK_BRG;
	params.rxclktype = SEAMAC_RXCLK_TTL;
	params.telement = SEAMAC_TELEMENT_BRG;
	params.parity = SEAMAC_PARITY_NONE;
	params.txbits = SEAMAC_BITS_8;
	params.rxbits = SEAMAC_BITS_8;
	params.addrfilter = 0xFF;
	params.addrrange = 0;
	params.crcpreset = SEAMAC_CRC_PRESET0;
	params.idlemode = SEAMAC_IDLE_FLAG;
	params.underrun = SEAMAC_UNDERRUN_FLAG;

	// NOTE: allow extra time for final character + 2 CRC + closing flag
	params.posttxdelay = (1000 * 8 * 4) / params.rate; 

	// If the device has software selectable interface, this will set it
	params.interface = SEAMAC_IF_232;

printf("after params\n");

	// set current device parameters
	rc = ioctl(fd, SEAMAC_IOCTL_SPARAMS, &params);
printf("back from ioctl\n");
	if (rc < 0) {
		printf("ioctl(SEAMAC_IOCTL_SPARAMS) on device %s"
		       " failed with err=%d %s\n",
		       devname, errno, strerror(errno));
		return rc;
	}
printf("1\n");
	// initialize send buffer
	for (i = 0; i < size; i++)
		buf[i] = (unsigned char) i;

printf("2\n");
	rc = write(fd, buf, size);
	if (rc < 0) {
		printf("write error=%d %s\n",
		       errno, strerror(errno));
		return rc;
	}
printf("done write\n");

	// This is especially important when transmitting back-to-back
	// frames.  If you do not allow enough time to pass between frames,
	// they can be concatenated into a single frame.
	rc = tcdrain(fd);
	printf("all data sent rc=%d\n", rc);

	return 0;
}

