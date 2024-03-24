#ifndef _PARAMS_H_
#define _PARAMS_H_

// maintain vsfs state in here
#include <limits.h>
#include <stdio.h>
#include "fly_swamp.h"

#define DEBUG 0

#define BLOCKSIZE 4096
#define NUM_OF_BLOCKS 64


struct my_state {
  FILE *logfile;
  int fsfile;
  char *fsfilename;
};
#define MY_DATA ((struct my_state *) fs_get_context()->private_data)

#endif
