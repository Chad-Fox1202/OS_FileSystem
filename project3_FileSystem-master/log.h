/*
  Modified by Jonathan Beaulieu 2017
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.
*/

#ifndef _LOG_H_
#define _LOG_H_
#include <stdio.h>

//  macro to log fields in structs.
#define log_struct(st, field, format, typecast) \
  log_msg("    " #field " = " #format "\n", typecast st->field)

FILE *log_open(void);
void log_msg(const char *format, ...);
int log_error(char *func);
void log_retstat(char *func, int retstat);
void log_stat(struct stat *si);

#endif
