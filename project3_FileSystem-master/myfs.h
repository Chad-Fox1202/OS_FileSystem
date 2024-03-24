#ifndef _MYFS_H_
#define _MYFS_H_

#include "disk.h"

typedef char pathlist[MAX_PATH_DEPTH][MAX_FILENAME + 1];

uint get_next_free_block();
uint get_next_free_inode();
uint get_inode_for_path(const char *path);
uint get_parent_dir_inode(const char *path);
int get_file_from_path(const char *path, char **filename);
int add_rec_to_dir_inode(uint inodenum, dirrec *rec);



#endif
