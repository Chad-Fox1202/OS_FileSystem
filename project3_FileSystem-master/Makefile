# specify all source files here
SRCS = myfs.c fly_swamp.c log.c disk.c

# specify targets here (names of executables)
TARG = fly_swamp

# specify compiler, flags and libs here
CC = gcc
OPTS = -Wall -O0 -g
OBJS = $(SRCS:.c=.o)
LIBS = # No libs required

# 'all' is not really needed
all: $(TARG)

# generates target executable
$(TARG): $(OBJS)
	$(CC) -o $(TARG) $(OBJS) $(LIBS)

# generates all object files from matching c files
%.o: %.c
	$(CC) $(OPTS) -c $< -o $@

# cleans up stuff
clean:
	rm -f fs.iso myfs.log
	rm -f $(OBJS) $(TARG) *~

clean-all: clean
	rm -fr swamps flies

log:
	tail -c +0 -F myfs.log

