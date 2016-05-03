/* 
 * Copyright (c) 2015-2016, Gregory M. Kurtzer. All rights reserved.
 * 
 * “Singularity” Copyright (c) 2016, The Regents of the University of California,
 * through Lawrence Berkeley National Laboratory (subject to receipt of any
 * required approvals from the U.S. Dept. of Energy).  All rights reserved.
 * 
 * If you have questions about your rights to use or distribute this software,
 * please contact Berkeley Lab's Innovation & Partnerships Office at
 * IPO@lbl.gov.
 * 
 * NOTICE.  This Software was developed under funding from the U.S. Department of
 * Energy and the U.S. Government consequently retains certain rights. As such,
 * the U.S. Government has been granted for itself and others acting on its
 * behalf a paid-up, nonexclusive, irrevocable, worldwide license in the Software
 * to reproduce, distribute copies to the public, prepare derivative works, and
 * perform publicly and display publicly, and to permit other to do so. 
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include <linux/loop.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h> 
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "config.h"
#include "loop-control.h"
#include "util.h"

#ifndef LO_FLAGS_AUTOCLEAR
#define LO_FLAGS_AUTOCLEAR 4
#endif



char * obtain_loop_dev(void) {
    char * loop_device;
    int loop_control;
    int devnum;

    //printf("Opening loop-control device\n");
    if ( ( loop_control = open("/dev/loop-control", O_RDWR)) < 0 ) {
        fprintf(stderr, "ERROR: Could not open loop-control device: %s\n", strerror(errno));
        return(NULL);
    }

    //printf("Sending loop-control LOOP_CTL_GET_FREE\n");
    if ( ( devnum = ioctl(loop_control, LOOP_CTL_GET_FREE) ) < 0 ) {
        fprintf(stderr, "ERROR: Could not get a loop device number: %s\n", strerror(errno));
        return(NULL);
    }
    //printf("Got new loop device number: %d\n", devnum);

    close(loop_control);

    loop_device = (char*) malloc(intlen(devnum) + 12);
    snprintf(loop_device, intlen(devnum) + 11, "/dev/loop%d", devnum);

    //printf("Checking for loop device: %s\n", *loop_device);
    if ( is_blk(loop_device) < 0 ) {
        //printf("Creating loop device: %s\n", *loop_device);
        if ( mknod(loop_device, S_IFBLK | 0644, makedev(7, devnum)) < 0 ) {
            //fprintf(stderr, "Could not create %s: %s\n", *loop_device, strerror(errno));
            return(NULL);
        }
    }

    return(loop_device);
}



int associate_loop_dev(char * image_path, char * loop_device) {
    int image_fd;
    int loop_fd;
    struct loop_info64 lo64 = {0};
    int offset = 0;

    lo64.lo_flags = LO_FLAGS_AUTOCLEAR;
    strncpy((char*)lo64.lo_file_name, image_path, LO_NAME_SIZE);
    lo64.lo_offset = offset;

    //printf("Opening image: %s\n", image_path);
    if ( (image_fd = open(image_path, O_RDWR)) < 0 ) {
        fprintf(stderr, "ERROR: Could not open image %s: %s\n", image_path, strerror(errno));
        return(-1);
    }

    //printf("Opening loop device: %s\n", loop_device);
    if ( ( loop_fd = open(loop_device, O_RDWR) ) < 0 ) {
        fprintf(stderr, "ERROR: Failed to open %s: %s\n", loop_device, strerror(errno));
        return(-1);
    }

    //printf("Associating image to loop device\n");
    if ( ioctl(loop_fd, LOOP_SET_FD, image_fd) < 0 ) {
        fprintf(stderr, "ERROR: Failed to associate image to loop\n");
        return(-1);
    }

    if ( ioctl(loop_fd, LOOP_SET_STATUS64, &lo64) < 0 ) {
        (void)ioctl(loop_fd, LOOP_CLR_FD, 0);
        fprintf(stderr, "ERROR: Failed to set loop flags on %s: %s\n", loop_device, strerror(errno));
        return(-1);
    }

    return(0);
}

