/*
  Modified by Jonathan Beaulieu 2017

  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  Since the point of this filesystem is to learn FUSE and its
  datastructures, I want to see *everything* that happens related to
  its data structures.  This file contains macros and functions to
  accomplish this.
*/

#include "params.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "log.h"

FILE *log_open() {
  FILE *logfile;

  // very first thing, open up the logfile and mark that we got in
  // here.  If we can't open the logfile, we're dead.
  logfile = fopen("myfs.log", "w");
  if (logfile == NULL) {
    perror("logfile");
    exit(EXIT_FAILURE);
  }

  // set logfile to line buffering
  setvbuf(logfile, NULL, _IOLBF, 0);

  return logfile;
}

void log_msg(const char *format, ...) {
  va_list ap;
  va_start(ap, format);

  vfprintf(MY_DATA->logfile, format, ap);
}

// Report errors to logfile and give -errno to caller
int log_error(char *func) {
  int ret = -errno;

  log_msg("    ERROR %s: %s\n", func, strerror(errno));

  return ret;
}

void log_retstat(char *func, int retstat) {
  int errsave = errno;
  log_msg("    %s returned %d\n", func, retstat);
  errno = errsave;
}

// make a system call, checking (and reporting) return status and
// possibly logging error
int log_syscall(char *func, int retstat, int min_ret) {
  log_retstat(func, retstat);

  if (retstat < min_ret) {
    log_error(func);
    retstat = -errno;
  }

  return retstat;
}

// This dumps the info from a struct stat.  The struct is defined in
// <bits/stat.h>; this is indirectly included from <fcntl.h>
void log_stat(struct stat *si) {
  log_msg("    si:\n");

  //  dev_t     st_dev;     /* ID of device containing file */
  log_struct(si, st_dev, %lld,);

  //  ino_t     st_ino;     /* inode number */
  log_struct(si, st_ino, %lld,);

  //  mode_t    st_mode;    /* protection */
  log_struct(si, st_mode, 0 % o,);

  //  nlink_t   st_nlink;   /* number of hard links */
  log_struct(si, st_nlink, %d,);

  //  uid_t     st_uid;     /* user ID of owner */
  log_struct(si, st_uid, %d,);

  //  gid_t     st_gid;     /* group ID of owner */
  log_struct(si, st_gid, %d,);

  //  dev_t     st_rdev;    /* device ID (if special file) */
  log_struct(si, st_rdev, %lld,);

  //  off_t     st_size;    /* total size, in bytes */
  log_struct(si, st_size, %lld,);

  //  blksize_t st_blksize; /* blocksize for filesystem I/O */
  log_struct(si, st_blksize, %ld,);

  //  blkcnt_t  st_blocks;  /* number of blocks allocated */
  log_struct(si, st_blocks, %lld,);

  //  time_t    st_atime;   /* time of last access */
  log_struct(si, st_atime, 0x % 08lx,);

  //  time_t    st_mtime;   /* time of last modification */
  log_struct(si, st_mtime, 0x % 08lx,);

  //  time_t    st_ctime;   /* time of last status change */
  log_struct(si, st_ctime, 0x % 08lx,);

}
