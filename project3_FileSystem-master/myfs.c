// Authors: Eric Bratsch, Chad Fox
// This program implements Very Simple File System (VSFS) based on http://pages.cs.wisc.edu/~remzi/OSFEP/file-implementation.pdf


#include "params.h"
#include "fly_swamp.h"
#include "myfs.h"

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "log.h"
#include "disk.h"


uint get_next_free_block() {
  d_bmap block_bitmap;
    int err = get_d_bmap(block_bitmap);
    if (err == -1) {
        // Handle error reading block bitmap
        return -1;
    }

    // Find the first free block in the block bitmap
    uint free_block = 0;
    
    while(block_bitmap[free_block]) {
      free_block++;
    }

    block_bitmap[free_block] = 1;
    
    // Update the block bitmap with the modified bitmap
    err = set_d_bmap(block_bitmap);
    if (err == -1) {
        // Handle error setting block bitmap
        return -1;
    }

    return free_block;
}

uint get_next_free_inode() {
  i_bmap inode_bitmap;
    int err = get_i_bmap(inode_bitmap);
    if (err == -1) {
        // Handle error reading inode bitmap
        return -1;
    }

    // Find the first free inode in the inode bitmap
    uint free_inode = 2;

   while(inode_bitmap[free_inode]){
    free_inode++;
   }

   inode_bitmap[free_inode] = 1;

    // Update the inode bitmap with the modified bitmap
    err = set_i_bmap(inode_bitmap);
    if (err == -1) {
        // Handle error setting inode bitmap
        return -1;
    }
    return free_inode;
}

int my_mknod(const char *path) {
  int retstat = 0;
  log_msg("my_mknod(path=\"%s\")\n", path);

  char *filename;
  uint parent_inode_num;


  parent_inode_num  = get_parent_dir_inode(path);
  get_file_from_path(path, (char **) &filename);

  int nextFreeInode = get_next_free_inode();
  inode *newInode = (inode *) malloc(sizeof(inode));
  get_inode(nextFreeInode, newInode);
  newInode->type = TYPE_FILE;
  newInode->size = 0;
  newInode->blocks = 0;
  set_inode(nextFreeInode, newInode);

  dirrec *dir = (dirrec *) malloc(sizeof(dirrec));
  dir->inum = nextFreeInode;
  strncpy(dir->name, filename, MAX_FILENAME);

  add_rec_to_dir_inode(parent_inode_num, dir);
  log_msg("    Parent dir inode: %u Filename: '%s'\n", parent_inode_num, filename);

  return retstat;
}

int my_read(uint inodenum, char *buf, uint size, uint offset) {
    int retstat = 0;
    log_msg("my_read(inum=%u, buf=0x%08x, size=%u, offset=%u)\n", inodenum, buf, size, offset);

    inode *mallNode = (inode*) malloc(sizeof(inode));

    int err = get_inode(inodenum, mallNode);
    if(err != 0){
        return -1;
    }
    uint start_block = offset / BLOCKSIZE;
    uint end_block = (offset + size - 1) / BLOCKSIZE;

    // Adjust size if it exceeds file size
    uint file_size = mallNode->size;
    if (offset >= file_size) {
        free(mallNode);
        return retstat;
    }
    if (offset + size > file_size) {
        size = file_size - offset;
    }


    // Loop through the blocks and read data
    for (uint current_block = start_block; current_block <= end_block; ++current_block) {
        block r_block;
        uint block_offset = (current_block == start_block) ? offset % BLOCKSIZE : 0;
        uint bytes_to_read = (size < BLOCKSIZE - block_offset) ? size : BLOCKSIZE - block_offset;

        // Read the block
        if (read_block(mallNode->pointers[current_block], r_block) != 0) {
            free(mallNode);
            return -1;
        }
        // Copy data from the block to the buffer
        memcpy(&buf[retstat], &r_block[block_offset], bytes_to_read);

        retstat += bytes_to_read;
        size -= bytes_to_read;
    }

    free(mallNode);

    return retstat;
}



int my_write(uint inodenum, char *buf, uint size, uint offset) {
  int bytesWritten = 0;
  log_msg("my_write(inum=%u, buf=0x%08x, size=%u, offset=%u)\n", inodenum, buf, size, offset);
  int numBlocksNeeded = ((size + offset + BLOCKSIZE - 1) / BLOCKSIZE); 

  inode * fileInode = (inode *) malloc(sizeof(inode));
  get_inode(inodenum, fileInode);

  block zeros;      
  memset(zeros, 0, sizeof zeros);
  while(fileInode->blocks < numBlocksNeeded){
    fileInode->blocks += 1;
    for(int i = 0; i < sizeof(fileInode->pointers) / sizeof(fileInode->pointers[0]); i++){
      if(fileInode->pointers[i] == 0){
        fileInode->pointers[i] = get_next_free_block();
        write_block(fileInode->pointers[i], zeros);
        break;
      }
    }
  }
  
  int currentBlock = offset / BLOCKSIZE;
  int blockOffset = offset % BLOCKSIZE;
  
  int dataOffset = 0;
  int numBytesToCopy;
  
  block dataBlock;
  fileInode->size = size + offset;

  while(size > 0){
    if(size >= BLOCKSIZE - blockOffset){  
      numBytesToCopy = BLOCKSIZE - blockOffset;
    }
    else{
      numBytesToCopy = size;
    }

    read_block(fileInode->pointers[currentBlock], dataBlock);
    memcpy((void *) &dataBlock[blockOffset], (void *) &buf[dataOffset], numBytesToCopy);
    write_block(fileInode->pointers[currentBlock], dataBlock);

    blockOffset = 0;
    dataOffset += numBytesToCopy;
    bytesWritten += numBytesToCopy;

    if(size > BLOCKSIZE){
      size -= BLOCKSIZE;
      currentBlock += 1;
    }
    else{
      size = 0;
    }
  }
  set_inode(inodenum, fileInode);
  free(fileInode);

  return bytesWritten;
}

int read_dir_from_inode(dirrec *first, uint inodenum) {
  int err;
  inode *dirinode = (inode *) malloc(sizeof(inode));
  err = get_inode(inodenum, dirinode);
  if (err == -1) {
    free(dirinode);
    return err;
  }

  err = read_dir_from_blocks(first, dirinode->size, dirinode->blocks, dirinode->pointers);
  if (err == -1) {
    free(dirinode);
    return err;
  }

  free(dirinode);
  return 0;
}

int add_rec_to_dir_inode(uint inodenum, dirrec *rec) {
  int err;
  inode *dirinode = (inode *) malloc(sizeof(inode));
  err = get_inode(inodenum, dirinode);
  if (err == -1) {
    free(dirinode);
    return err;
  }

  dirrec *first = (dirrec *) malloc(sizeof(dirrec));
  read_dir_from_inode(first, inodenum);
  rec->next = first;
  first = rec;

  err = write_dir_to_blocks(first, dirinode->blocks, dirinode->pointers, &dirinode->size);
  if (err == -1) {
    free_dirrec_list(first);
    free(dirinode);
    return err;
  }
  if (err > 0) {
    for (int i = 0; i < err; ++i) {
      dirinode->pointers[i + dirinode->blocks] = get_next_free_block();
    }
    dirinode->blocks += err;

    // Retry write with more blocks.
    err = write_dir_to_blocks(first, dirinode->blocks, dirinode->pointers, &dirinode->size);
    if (err == -1) {
      free_dirrec_list(first);
      free(dirinode);
      return err;
    }
  }

  // Update inode.
  set_inode(inodenum, dirinode);

  free_dirrec_list(first);
  free(dirinode);
  return 0;
}

int split_path(const char *path, size_t pathlen, pathlist pathl) {
  if (pathlen < 1) return -1;
  if (path[0] != '/') return -1;

  int i = 0;
  uint cur = 1;
  uint prev = 1;
  while (i < MAX_PATH_DEPTH && cur <= pathlen) {
    if (path[cur] == '/') {
      if (cur - prev > MAX_FILENAME) {
        return -1;
      }
      memcpy(pathl[i], path + prev, cur - prev);
      pathl[i][cur - prev] = '\0';
      prev = cur + 1;
      i++;
    }
    cur++;
  }

  // Take care of the last one
  if (cur - prev > 0) {
    memcpy(pathl[i], path + prev, cur - prev - 1);
    pathl[i][cur - prev - 1] = '\0';
    i++;
  }
  pathl[i][0] = '\0';

  return 0;
}

uint get_parent_dir_inode(const char *path) {
  size_t pathlen = strnlen(path, MAX_FILENAME * MAX_PATH_DEPTH);
  size_t last_slash = pathlen;
  while (path[last_slash] != '/') {
    last_slash--;
  }

  char path_dir[last_slash + 1];
//  char path_file[pathlen - last_slash];
  memcpy(path_dir, path, last_slash);
  path_dir[last_slash] = '\0';
//  memcpy(path_file, path + last_slash + 1, pathlen - last_slash - 1);
//  path_file[pathlen - last_slash - 1] = '\0';

//  log_msg("Path dir: '%s' Path file: '%s'\n", path_dir, path_file);

  uint dir_inode_num = get_inode_for_path(path_dir);
  return dir_inode_num;
}

uint get_inum_for_name_in_dir(dirrec *root, char *name) {
  dirrec *cur = root;
  while (cur) {
    if (strncmp(cur->name, name, MAX_FILENAME) == 0) {
      return cur->inum;
    }
    cur = cur->next;
  }
  return 0;
}

uint get_inode_for_path(const char *path) {
  pathlist pl;
  size_t pathlen = strnlen(path, MAX_PATH_DEPTH*MAX_FILENAME);
  split_path(path, pathlen, pl);

  int i = 1;
  char *next;
  next = pl[0];
  uint cur_inode_num = 2;
  inode *cur_inode = (inode *) malloc(sizeof(inode));

  while (next[0]) {
    get_inode(cur_inode_num, cur_inode);
    if (cur_inode->type == TYPE_DIR) {
      dirrec *curdir = (dirrec *) malloc(sizeof(dirrec));
      read_dir_from_inode(curdir, cur_inode_num);
      cur_inode_num = get_inum_for_name_in_dir(curdir, next);
      if (cur_inode_num < 2) {
        return 0;
      }
      free_dirrec_list(curdir);
    } else {
      return 0;
    }
    next = pl[i];
    i++;
  }

  free(cur_inode);

  return cur_inode_num;
}

int get_file_from_path(const char *path, char **filename) {
  size_t pathlen = strnlen(path, MAX_FILENAME * MAX_PATH_DEPTH);
  size_t last_slash = pathlen;
  while (path[last_slash] != '/') {
    last_slash--;
  }

  size_t filename_len = pathlen - last_slash;
  *filename = (char*)malloc(sizeof(char)*filename_len);
  memcpy(*filename, path + last_slash + 1, pathlen - last_slash - 1);
  (*filename)[pathlen - last_slash - 1] = '\0';

  return 0;
}

void myfs_usage() {
  fprintf(stderr, "usage: ./file_swamp fsFile\n\tThen enter commands");
  abort();
}


int my_mkdir(const char *path) {
  int retstat = 0;
  int err;
  log_msg("my_mkdir(path=\"%s\")\n", path);

  size_t pathlen = strnlen(path, MAX_FILENAME * MAX_PATH_DEPTH);
  size_t last_slash = pathlen;
  while (path[last_slash] != '/') {
    last_slash--;
  }

  char path_dir[last_slash + 1];
  char path_file[pathlen - last_slash];
  memcpy(path_dir, path, last_slash);
  path_dir[last_slash] = '\0';
  memcpy(path_file, path + last_slash + 1, pathlen - last_slash - 1);
  path_file[pathlen - last_slash - 1] = '\0';

  log_msg("Path dir: '%s' Path file: '%s'\n", path_dir, path_file);

  uint dir_inode_num = get_inode_for_path(path_dir);
  dirrec *head = (dirrec *) malloc(sizeof(dirrec));
  dirrec *newrec = (dirrec *) malloc(sizeof(dirrec));

  read_dir_from_inode(head, dir_inode_num);

  strncpy(newrec->name, path_file, pathlen - last_slash);
  newrec->inum = get_next_free_inode();

  inode newInode;
  newInode.type = TYPE_DIR;
  // newInode.size is set when we call write_dir_to_blocks
  newInode.blocks = 1;
  newInode.pointers[0] = get_next_free_block();

  // set up directory data
  dirrec first;
  first.name[0] = '.';
  first.name[1] = '\0';
  first.inum = newrec->inum;
  first.next = (dirrec *) malloc(sizeof(dirrec));
  first.next->name[0] = '.';
  first.next->name[1] = '.';
  first.next->name[2] = '\0';
  first.next->inum = dir_inode_num;
  first.next->next = NULL;


  err = write_dir_to_blocks(&first, 1, newInode.pointers, &newInode.size);
  if (err == -1) {
    log_msg("Could not write dir.");
    abort();
  }

  err = set_inode(newrec->inum, &newInode);
  if (err == -1) {
    log_msg("Could not set inode %u.", newrec->inum);
    abort();
  }

  add_rec_to_dir_inode(dir_inode_num, newrec);

  free_dirrec_list(first.next);

  return retstat;
}

int my_open(const char *path, uint *fd) {
  int retstat = 0;
  log_msg("my_open(path=\"%s\", fd=0x%08x)\n", path, fd);

  uint inodenum = get_inode_for_path(path);
  if (inodenum < 2) {
    retstat = -1;
  }
  *fd = inodenum;
  log_msg("   fd = %u\n", *fd);
  return retstat;
}

int my_getattr(const char *path, struct stat *statbuf) {
  int retstat = 0;
  log_msg("my_getattr(path=\"%s\", statbuf=0x%08x)\n", path, statbuf);

  uint inodenum = get_inode_for_path(path);
  if (inodenum < 2) {
    log_msg("    No such file or directory.\n");
    return -ENOENT;
  }
  inode *ino = (inode *) malloc(sizeof(inode));
  get_inode(inodenum, ino);
  statbuf->st_dev = 0;
  statbuf->st_ino = inodenum;
  statbuf->st_mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
  if (ino->type == TYPE_DIR) {
    statbuf->st_mode |= S_IFDIR;
  } else {
    statbuf->st_mode |= S_IFREG;
  }
  statbuf->st_nlink = 1;
  statbuf->st_blksize = BLOCKSIZE;
  statbuf->st_blocks = ino->blocks;
  statbuf->st_size = ino->size;

  log_stat(statbuf);
  free(ino);
  return retstat;
}

void *my_init() {
  log_msg("my_init()\n");
  int err;

  // Init file system if not already exists
  if (access(MY_DATA->fsfilename, F_OK) == -1) {
    // fsfile doesn't exist
    MY_DATA->fsfile = open(MY_DATA->fsfilename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG);
    fcntl(MY_DATA->fsfile, F_SETFL, O_NONBLOCK);
    err = ftruncate(MY_DATA->fsfile, BLOCKSIZE * NUM_OF_BLOCKS);
    if (err == -1) {
      log_msg("Could not write to fs file.");
      abort();
    }
    fsync(MY_DATA->fsfile);
    log_msg("    Created new FS File\n");

    d_bmap dmap;
    err = get_d_bmap(dmap);
    if (err == -1) {
      log_msg("Could not get d_bmap.");
      abort();
    }
    for (size_t i = 0; i < 9; i++) {
      dmap[i] = 1;
    }
    err = set_d_bmap(dmap);
    if (err == -1) {
      log_msg("Could not set d_bmap.");
      abort();
    }

    i_bmap imap;
    err = get_i_bmap(imap);
    if (err == -1) {
      log_msg("Could not get i_bmap.");
      abort();
    }
    imap[2] = 1;
    err = set_i_bmap(imap);
    if (err == -1) {
      log_msg("Could not set i_bmap.");
      abort();
    }

    // Setup root inode
    inode rootnode;
    rootnode.type = TYPE_DIR;
    rootnode.size = 0;
    rootnode.blocks = 1;
    rootnode.pointers[0] = 8;

    // set up root data
    dirrec root;
    root.name[0] = '.';
    root.name[1] = '\0';
    root.inum = 2;
    root.next = (dirrec *) malloc(sizeof(dirrec));
    root.next->name[0] = '.';
    root.next->name[1] = '.';
    root.next->name[2] = '\0';
    root.next->inum = 2;
    root.next->next = NULL;


    err = write_dir_to_blocks(&root, 1, rootnode.pointers, &rootnode.size);
    if (err == -1) {
      log_msg("Could not write dir.");
      abort();
    }

    err = set_inode(2, &rootnode);
    if (err == -1) {
      log_msg("Could not set inode 2.");
      abort();
    }

    // Clean up
    free_dirrec_list(root.next);

  } else {
    log_msg("\tUsing old FS File\n");
    MY_DATA->fsfile = open(MY_DATA->fsfilename, O_RDWR);
    fcntl(MY_DATA->fsfile, F_SETFL, O_NONBLOCK);
  }

  return MY_DATA;
}

void my_destroy() {
  log_msg("my_destroy()\n");
  close(MY_DATA->fsfile);
}

struct fs_operations my_oper = {
    .mknod = my_mknod,
    .mkdir = my_mkdir,

    .open = my_open,
    .read = my_read,
    .write = my_write,

    .opendir = my_open,
    .getattr = my_getattr,

    .init = my_init,
    .destroy = my_destroy,
};


int main(int argc, char *argv[]) {
  int fs_stat;
  struct my_state *my_data;

  // If running as root, die!
  if ((getuid() == 0) || (geteuid() == 0)) {
    fprintf(stderr, "Running as root opens unacceptable security holes\n");
    return 1;
  }

  // Perform some sanity checking on the command line:  make sure
  // there are enough arguments, and that the last one does not start
  // with a hyphen (this will break if you actually have a rootpoint
  // whose name starts with a hyphen, but so will a zillion other
  // programs)
  if ((argc < 2) || (argv[argc - 1][0] == '-')) myfs_usage();

  my_data = malloc(sizeof(struct my_state));
  if (my_data == NULL) {
    perror("main alloc");
    abort();
  }

  // Pull the fsfile out of the argument list and save it in my
  // internal data
  if (realpath(argv[argc - 1], NULL) == NULL) {
    char *parentdir = realpath(".", NULL);
    char *file = argv[argc - 1];
    size_t parlen = strnlen(parentdir, 4096);
    size_t filelen = strnlen(file, 255);
    char *filepath = (char *) malloc(sizeof(char) * (parlen + filelen + 2));
    strncpy(filepath, parentdir, parlen);
    strncat(filepath, "/", 1);
    strncat(filepath, file, filelen);
    filepath[parlen + filelen + 1] = '\0';
    my_data->fsfilename = filepath;
  } else {
    my_data->fsfilename = realpath(argv[argc - 1], NULL);
  }
  argv[argc - 1] = NULL;
  argc--;
  if (DEBUG) fprintf(stderr, "Sizeof int: %zu\nSizeof size_t: %zu\nSizeof uint: %zu\n", sizeof(int), sizeof(size_t), sizeof(uint));

  my_data->logfile = log_open();

  // turn over control to fs (A home brewed version of fuse)
  if (DEBUG) fprintf(stderr, "about to call fs_main\n");
  fs_stat = fs_main(argc, argv, &my_oper, my_data);
  if (DEBUG) fprintf(stderr, "fs_main returned %d\n", fs_stat);

  return fs_stat;
}
