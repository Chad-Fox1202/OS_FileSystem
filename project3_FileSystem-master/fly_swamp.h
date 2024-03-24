#ifndef _FLY_SWAMP_H_
#define _FLY_SWAMP_H_

#include <sys/stat.h>

typedef unsigned int uint;

struct fs_operations {
    int (*mknod) (const char *);
    int (*read) (uint, char *, uint, uint);
    int (*write) (uint, char *, uint, uint);
    int (*open) (const char *, uint *);

    int (*mkdir) (const char *);
    int (*opendir) (const char *, uint *);
    int (*readdir) (uint, char *, uint);
    int (*getattr) (const char *, struct stat *);

    void *(*init) ();
    void (*destroy) ();
};

struct fs_context {
    void *private_data;
};

struct fs_context* fs_get_context();

int fs_main_real(int argc, char *argv[], const struct fs_operations *op, size_t op_size, void *user_data);

#define fs_main(argc, argv, op, user_data) \
        fs_main_real(argc, argv, op, sizeof(*(op)), user_data)

#endif