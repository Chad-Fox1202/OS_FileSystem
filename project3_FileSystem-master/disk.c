#include "params.h"
#include "disk.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int int_to_char_4(uint i, char buf[4]) {
  // Assumes Big Endian.
  buf[3] = (char) ((i >> 24) & 0xFF);
  buf[2] = (char) ((i >> 16) & 0xFF);
  buf[1] = (char) ((i >> 8) & 0xFF);
  buf[0] = (char) (i & 0xFF);
  return 0;
}

int round_up_to(uint i, uint units) {
  return i + (units - (i % units));
}

int read_block(uint blocknum, block data) {
  lseek(MY_DATA->fsfile, blocknum * BLOCKSIZE, SEEK_SET);
  ssize_t bytes_read = read(MY_DATA->fsfile, data, BLOCKSIZE);
  if (bytes_read != BLOCKSIZE) {
    return -1;
  }
  return 0;
}

int write_block(uint blocknum, block data) {
  lseek(MY_DATA->fsfile, (blocknum * BLOCKSIZE), SEEK_SET);
  ssize_t written = write(MY_DATA->fsfile, data, BLOCKSIZE);
  if (written != BLOCKSIZE) {
    return -1;
  }
  fsync(MY_DATA->fsfile);
  return 0;
}

int get_i_bmap(i_bmap map) {
  block data;
  if (read_block(I_BMAP_BLOCK, data) == 0) {
    memcpy(map, data, NUM_OF_INODES);
  } else {
    return -1;
  }
  return 0;
}

int set_i_bmap(i_bmap map) {
  block data;
  memset(data, '\0', BLOCKSIZE);
  memcpy(data, map, NUM_OF_INODES);
  return write_block(I_BMAP_BLOCK, data);
}

int get_d_bmap(d_bmap map) {
  block data;
  if (read_block(D_BMAP_BLOCK, data) == 0) {
    memcpy(map, data, NUM_OF_BLOCKS);
  } else {
    return -1;
  }
  return 0;
}

int set_d_bmap(d_bmap map) {
  block data;
  memset(data, '\0', BLOCKSIZE);
  memcpy(data, map, NUM_OF_BLOCKS);
  return write_block(D_BMAP_BLOCK, data);
}

int get_inode(uint inodenum, inode *node) {
  // TODO: handle errors
  uint table_block_offset = inodenum / (BLOCKSIZE / sizeof(inode));
  uint block_offset = (inodenum % (BLOCKSIZE / sizeof(inode))) * sizeof(inode);

  block data;
  read_block(table_block_offset + I_TABLE_START_BLOCK, data);
  memcpy((char *) node, (char *) data + block_offset, sizeof(inode));
  return 0;
}

int set_inode(uint inodenum, inode *node) {
  // TODO: handle errors
  uint table_block_offset = inodenum / (BLOCKSIZE / sizeof(inode));
  uint block_offset = (inodenum % (BLOCKSIZE / sizeof(inode))) * sizeof(inode);

  block data;
  read_block(table_block_offset + I_TABLE_START_BLOCK, data);

  memcpy((char *) data + block_offset, (char *) node, sizeof(inode));
  return write_block(table_block_offset + I_TABLE_START_BLOCK, data);
}

int read_dir_from_blocks(dirrec *first, uint size, uint blocks, uint *pointers) {
  char data[size + 1];

  if (size < 1) return -1;
  block cur_block;
  for (uint i = 0; i < size / BLOCKSIZE && i < blocks; i++) {
    int err = read_block(pointers[i], cur_block);
    if (err == -1) return err;
    memcpy(data + (i * BLOCKSIZE), cur_block, BLOCKSIZE);
  }
  if (size % BLOCKSIZE != 0) {
    // If there is a partial block.
    int err = read_block(pointers[size / BLOCKSIZE], cur_block);
    memcpy(data + (BLOCKSIZE * (size / BLOCKSIZE)), cur_block, size % BLOCKSIZE);
    if (err == -1) return err;
  }

  dirrec *cur = first;
  dirrec *prev = NULL;
  first->next = NULL;
  uint cur_index = 0;
  while (cur_index < size) {
    if (cur == NULL) {
      cur = (dirrec *) malloc(sizeof(dirrec));
      if (prev != NULL) {
        prev->next = cur;
      }
      cur->next = NULL;
    }
    cur->inum = *(uint *) (data + cur_index);
    cur_index += sizeof(int);
    uint reclen = *(uint *) (data + cur_index);
    cur_index += sizeof(uint);
    uint strlength = *(uint *) (data + cur_index);
    cur_index += sizeof(uint);
    memcpy(cur->name, data + cur_index, strlength);
    cur->name[strlength + 1] = '\0';
    cur_index += reclen;
    prev = cur;
    cur = cur->next;
  }

  return 0;
}

int write_dir_to_blocks(dirrec *first, uint blocks, uint *pointers, uint *size) {
  // Calculate length of dir.
  uint length = 0;
  dirrec *cur = first;
  while (cur != NULL) {
    length += 3 * sizeof(uint);
    uint strlength = (uint) (strnlen(cur->name, MAX_FILENAME) + 1);
    length += 4 * ((strlength + (4 - (strlength % 4))) / 4); // Round up to the nearest factor of 4.
    cur = cur->next;
  }

  char data[length];
  *size = length;

  cur = first;
  uint cur_index = 0;
  char buf[4];
  while (cur != NULL) {
    int_to_char_4(cur->inum, buf);
    memcpy(data + cur_index, buf, 4);
    cur_index += 4;

    uint strlength = (uint) (strnlen(cur->name, MAX_FILENAME) + 1); // Plus 1 for the null char that will be at the end.
    uint reclen = 4 * ((strlength + (4 - (strlength % 4))) / 4); // Round up to the nearest factor of 4.

    int_to_char_4(reclen, buf);
    memcpy(data + cur_index, buf, 4);
    cur_index += 4;
    int_to_char_4(strlength, buf);
    memcpy(data + cur_index, buf, 4);
    cur_index += 4;

    memcpy(data + cur_index, cur->name, strlength);
    cur_index += strlength;
    memcpy(data + cur_index, "\0", 1);
    cur_index = cur_index - strlength + reclen;
    cur = cur->next;
  }

  int err;
  // Save data to blocks
  if (blocks * BLOCKSIZE >= length) {
    // If we have enough space with the current number of blocks
    block buf_block;
    for (uint i = 0; i < length / BLOCKSIZE; i++) {
      // Save all whole blocks
      memcpy(buf_block, data + (BLOCKSIZE * i), BLOCKSIZE);
      err = write_block(pointers[i], buf_block);
      if (err == -1) return err;
    }
    if (length % BLOCKSIZE != 0) {
      // If there is a partial block.
      // TODO: rethink this!
      memcpy(buf_block, data + (BLOCKSIZE * (length / BLOCKSIZE)), length % BLOCKSIZE);
      err = write_block(pointers[length / BLOCKSIZE], buf_block);
      if (err == -1) return err;
    }
  } else {
    uint needed_space = length - (blocks * BLOCKSIZE);
    return round_up_to(needed_space, BLOCKSIZE) / BLOCKSIZE;
  }
  return 0;

//  return write_data_to_blocks(data, length, blocks, pointers);
}

int free_dirrec_list(dirrec *first) {
  if (first != NULL) {
    free_dirrec_list(first->next);
    free(first);
  }
  return 0;
}
