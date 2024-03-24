#ifndef _DISK_H_
#define _DISK_H_

#include "params.h"

#define NUM_OF_INODES 80
#define MAX_FILENAME 255
#define MAX_PATH_DEPTH 64

#define SUPER_BLOCK 0
#define I_BMAP_BLOCK 1
#define D_BMAP_BLOCK 2
#define I_TABLE_START_BLOCK 3

#define TYPE_DIR 0
#define TYPE_FILE 1

typedef struct __inode {
  uint type;
  uint size;
  uint blocks;
  uint pointers[61];
} inode;

typedef struct __dirrec {
  uint inum;
  char name[MAX_FILENAME + 1];
  struct __dirrec *next;
} dirrec;

typedef char i_bmap[NUM_OF_INODES];  // Inode Bitmap
typedef char d_bmap[NUM_OF_BLOCKS];  // Data Block Bitmap
typedef char block[BLOCKSIZE];

int read_block(uint blocknum, block data);

int write_block(uint blocknum, block data);

int get_i_bmap(i_bmap map);

int set_i_bmap(i_bmap map);

int get_d_bmap(d_bmap map);

int set_d_bmap(d_bmap map);

int get_inode(uint inodenum, inode *node);

int set_inode(uint inodenum, inode *node);

int read_dir_from_blocks(dirrec *first, uint size, uint blocks, uint *pointers);

// Returns
//         0 if all is good
//         -1 if error writing
//         number of more blocks needed to save the dir
int write_dir_to_blocks(dirrec *first, uint blocks, uint *pointers, uint *size);

int free_dirrec_list(dirrec *first);

#endif
