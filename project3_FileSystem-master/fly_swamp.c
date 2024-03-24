#include "params.h"
#include "fly_swamp.h"

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#define MAX_LINE_SIZE 262144
#define ERR if (DEBUG) {fprintf(stderr, "Will exit now!\n");}returnstat=1;running=0;break

// Based on code from: http://stackoverflow.com/questions/5403103/hex-to-ascii-string-conversion
int hex_to_char(char c){
  int first = c / 16 - 3;
  int second = c % 16;
  int result = first*10 + second;
  if(result > 9) result--;
  return result;
}

char hex_to_ascii(char c, char d){
  int high = hex_to_char(c) * 16;
  int low = hex_to_char(d);
  return (char)(high+low);
}

int hex_data_to_char_array(char *hex, uint size, char **data) {
  *data = (char*)malloc(sizeof(char)*(size/2));
  char buf = 0;
  for(int i = 0; i < size; i++){
    if(i % 2 != 0){
      (*data)[i/2] = hex_to_ascii(buf, hex[i]);
    }else{
      buf = hex[i];
    }
  }
  return 0;
}

struct fs_context cxt;

struct fs_context* fs_get_context() {
  return &cxt;
}

int fs_main_real(int argc, char *argv[], const struct fs_operations *op, size_t op_size, void *user_data) {
  char line[MAX_LINE_SIZE];
  char cmd;
  char *path;
  char *hexdata;
  char *data;
  uint size;
  uint offset;
  int ret;
  struct stat file_stat;

  cxt.private_data = user_data;

  cxt.private_data = op->init();

  int returnstat = 0;
  int running = 1;
  while (running) {
    if (!fgets(line, MAX_LINE_SIZE, stdin)) {
      // Unexpected EOF
      returnstat = 1;
      running = 0;
      break;
    }
    if (sscanf(line, "%c", &cmd) != 1) { // No command so skip line.
      continue;
    }
    switch (cmd) {
      // TODO: Fix this to handle names with spaces.
      case 'e': // Exit
        running = 0;
        break;
      case 't': // Touch
        sscanf(line, "%*c %ms", &path);
        if (DEBUG) fprintf(stderr, "Touching '%s'...\n", path);

        ret = op->mknod(path);

        if (ret != 0) {
          if (DEBUG) fprintf(stderr, "mknod returned an error code: %d!\n", ret);
          ERR;
        }

        free(path);
        break;
      case 'd': // mkdir
        sscanf(line, "%*c %ms", &path);
        if (DEBUG) fprintf(stderr, "Making directory '%s'...\n", path);

        ret = op->mkdir(path);

        if (ret != 0) {
          if (DEBUG) fprintf(stderr, "mkdir returned an error code: %d!\n", ret);
          ERR;
        }

        free(path);
        break;
      case 'r': // Read
        sscanf(line, "%*c %ms %u %u", &path, &offset, &size);
        if (DEBUG) fprintf(stderr, "Reading %u bytes from '%s' @ %u...\n", size, path, offset);

        if (size < 0) {
          if (DEBUG) fprintf(stderr, "size is too small\n");
          ERR;
        }

        if (size > MAX_LINE_SIZE) {
          if (DEBUG) fprintf(stderr, "size is too large\n");
          ERR;
        }

        ret = op->getattr(path, &file_stat);

        if (ret != 0) {
          if (DEBUG) fprintf(stderr, "getattr returned an error code: %d!\n", ret);
          ERR;
        }
        data = (char*)malloc(sizeof(char)*size);
        ret = op->read((uint) file_stat.st_ino, data, size, offset);

        if (ret != size) {
          // This isn't an error just means we read passed the end of the file's data.
          if (DEBUG) fprintf(stderr, "read returned '%d' != size '%d'!\n", ret, size);
        }

        // Print the output in hex
        for (int i = 0; i < ret; ++i) {
          fprintf(stdout, "%02X", data[i]);
        }
        fprintf(stdout, "\n");

        free(data);
        free(path);
        break;
      case 'w': // Write
        sscanf(line, "%*c %ms %u %u %ms", &path, &offset, &size, &hexdata);
        if (strnlen(hexdata, MAX_LINE_SIZE) != size*2) {
          if (DEBUG) fprintf(stderr, "Writing Failed because size does not match the length of the hex data!\n");
          ERR;
        }
        hex_data_to_char_array(hexdata, size*2, &data);
        if (DEBUG) fprintf(stderr, "Writing %u bytes to '%s' @ %u...\n", size, path, offset);

        ret = op->getattr(path, &file_stat);

        if (ret != 0) {
          if (DEBUG) fprintf(stderr, "getattr returned an error code: %d!\n", ret);
          ERR;
        }

        ret = op->write((uint)file_stat.st_ino, data, size, offset);

        if (ret != size) {
          if (DEBUG) fprintf(stderr, "write returned '%d' != size '%d'!\n", ret, size);
          ERR;
        }

        free(hexdata);
        free(path);
        free(data);
        break;
      default:
        break;
    }
    if (DEBUG) fprintf(stderr, "Good\n");
  }

  op->destroy();

  return returnstat;
}