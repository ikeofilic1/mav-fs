#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define BLOCK_SIZE 1024
#define BLOCKS_PER_FILE 1024

#define MAX_FILE_LEN 64
#define NUM_FILES 256

#define DISK_IMAGE_SIZE 67108864
#define NUM_BLOCKS (DISK_IMAGE_SIZE) / (BLOCK_SIZE)

uint8_t curr_image[NUM_BLOCKS][BLOCK_SIZE];

struct directoryEntry
{
    char filename[64];
    bool in_use;
    int32_t inode;
};

struct directoryEntry *directory;

struct inode
{
    int32_t blocks[BLOCKS_PER_FILE];
    bool in_use;
};

int main()
{
}
