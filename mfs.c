#define _GNU_SOURCE 1

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define BLOCK_SIZE 1024
#define BLOCKS_PER_FILE 1024

#define MAX_FILE_LEN 64
#define NUM_FILES 256

#define FIRST_DATA_BLOCK 341

#define DISK_IMAGE_SIZE 67108864
#define NUM_BLOCKS (DISK_IMAGE_SIZE / BLOCK_SIZE)
#define MAX_FILE_SIZE (BLOCK_SIZE * BLOCKS_PER_FILE)
#define USABLE_SIZE ((NUM_BLOCKS - (FIRST_DATA_BLOCK - 1)) * 1024)

// No command has more than 5 arguments in our
// list of commands
#define MAX_NUM_ARGUMENTS 5
#define MAX_COMMAND_SIZE 255

#define ATTRIB_HIDDEN 0x1
#define ATTRIB_R_ONLY 0x2

#define CIPHER_SIZE 1

///////////////////////////////////////
// Forward declarations
//////////////////////////////////////
void init(void);
void parse_tokens(const char *command_string, char **token);
void free_array(char **arr, size_t size);
void insert(char *tokens[MAX_NUM_ARGUMENTS]);
void retrieve(char *tokens[MAX_NUM_ARGUMENTS]);
void readfile(char *tokens[MAX_NUM_ARGUMENTS]);
void del(char *tokens[MAX_NUM_ARGUMENTS]);
void undel(char *tokens[MAX_NUM_ARGUMENTS]);
void list(char *tokens[MAX_NUM_ARGUMENTS]);
void openfs(char *tokens[MAX_NUM_ARGUMENTS]);
void closefs(char *tokens[MAX_NUM_ARGUMENTS]);
void createfs(char *tokens[MAX_NUM_ARGUMENTS]);
void savefs(char *tokens[MAX_NUM_ARGUMENTS]);
void attrib(char *tokens[MAX_NUM_ARGUMENTS]);
void encrypt(char *tokens[MAX_NUM_ARGUMENTS]);
void decrypt(char *tokens[MAX_NUM_ARGUMENTS]);
void df(char *tokens[MAX_NUM_ARGUMENTS]);

uint8_t curr_image[NUM_BLOCKS][BLOCK_SIZE];

uint8_t *free_blocks;
uint8_t *free_inodes;

uint32_t size_avail;
uint8_t image_open;
char image_name[256];

struct directoryEntry
{
    char filename[MAX_FILE_LEN];
    bool in_use;
    int32_t inode;
};

struct inode
{
    int32_t blocks[BLOCKS_PER_FILE];
    bool in_use;
    uint8_t attribute;
    uint32_t file_size;
};

struct inode *inodes;
struct directoryEntry *directory;

// Command stuff
typedef void (*command_fn)(char *[MAX_NUM_ARGUMENTS]);

typedef struct _command
{
    char *name;
    command_fn run;
    uint8_t num_args;
} command;

// As of now, we only have 14 commands
#define NUM_COMMANDS 14

// We use a table to store and lookup command names and their corresponding functions.
// Essentially, this is a map/dictionary that is highly modular (compared to a massive
// switch statement). Also, if the table decides to grow larger, we can always store the
// elements in order of their keys (the name field), and use binary search when looking up a
// command as opposed to linearly scanning the commands table

static const command commands[NUM_COMMANDS] = {
    //  cmd name	call back	min arguments

    {"insert", insert, 1},
    {"retrieve", retrieve, 1},
    {"read", readfile, 3},
    {"del", del, 1},
    {"undel", undel, 1},
    {"list", list, 0},
    {"df", df, 0},
    {"open", openfs, 1},
    {"close", closefs, 0},
    {"createfs", createfs, 1},
    {"savefs", savefs, 0},
    {"attrib", attrib, 2},
    {"encrypt", encrypt, 2},
    {"decrypt", decrypt, 2},
};
// End of command stuff

int32_t findFreeBlock()
{
    int i;
    for (i = FIRST_DATA_BLOCK; i < NUM_BLOCKS; i++)
    {
        if (free_blocks[i])
        {
            free_blocks[i] = 0;
            return i;
        }
    }
    return -1;
}

int32_t findFreeInode()
{
    int i;
    for (i = 0; i < NUM_FILES; i++)
    {
        if (free_inodes[i])
        {
            assert(!inodes[i].in_use);

            free_inodes[i] = 0;
            return i;
        }
    }
    return -1;
}

int32_t findFreeDirectory()
{
    int i;
    for (i = 0; i < NUM_FILES; ++i)
    {
        if (directory[i].in_use == 0) // we found one
        {
            return i;
        }
    }
    return -1; // All spots in the directory are taken
}

int32_t find_file_by_name(char *name, uint8_t *dir)
{
    int32_t inode_num = -1;

    int i = 0;
    for (; i < NUM_FILES; ++i)
    {
        if (directory[i].in_use && !strncmp(name, directory[i].filename, 64))
        {
            inode_num = directory[i].inode;
            break;
        }
    }

    if (dir)
        *dir = i;

    return inode_num;
}

void xor_file(uint32_t inode, uint8_t cipher)
{
    int block_idx = 0;

    // Go through each block that this file uses
    while (block_idx < BLOCKS_PER_FILE)
    {
        uint32_t block_num = inodes[inode].blocks[block_idx];

        // We have reached the end of the file
        if (block_num == -1)
            break;

        // XOR the block
        for (int i = 0; i < BLOCK_SIZE; ++i)
        {
            curr_image[block_num][i] ^= cipher;
        }

        block_idx++;
    }
}

// copy a file into the disk image
void insert(char *tokens[MAX_NUM_ARGUMENTS])
{
    if (!image_open)
    {
        printf("insert: ERROR: Disk image not open.\n");
        return;
    }

    char *filename = tokens[1];

    // We do not want slashes in our file name though that does not really constitute a problem
    // At least support adding files from different directory on the host machine to the image
    // I have to only use the basename of the filename since we support only one-level directory
    char *base = basename(filename);

    if (strlen(base) > 64)
    {
        fprintf(stderr, "insert error: filename too long\n");
        return;
    }

    int32_t inode = find_file_by_name(base, NULL);
    if (inode != -1)
    {
        fprintf(stderr, "ERROR: file already exists\n");
        return;
    }

    // verify the file exists on host disk
    struct stat buf;
    int ret = stat(filename, &buf);
    if (ret == -1)
    {
        printf("ERROR: file does not exist.\n");
        return;
    }
    // verify that the file isn't too big
    if (buf.st_size > MAX_FILE_SIZE)
    {
        printf("ERROR: file exceeds maximum size.\n");
        return;
    }
    // verify that there is enough space
    if (buf.st_size > size_avail)
    {
        printf("ERROR: there is not enough space for a file of this size.\n");
        return;
    }

    // find an empty directory entry
    int directory_entry = findFreeDirectory();
    if (directory_entry == -1) // then we never found a valid one :(
    {
        printf("ERROR: no empty directory entry found.\n");
        return;
    }
    // Find an unused inode
    int32_t inode_index = findFreeInode();
    if (inode_index == -1)
    {
        printf("ERROR: could not find a free inode.\n");
        return;
    }

    // Save off the size of the input file
    // and initialize index variables to zero
    int32_t copy_size = buf.st_size;
    int32_t offset = 0;
    int32_t block_index = 0;
    int32_t inode_block = 0;

    // open the input file read-only
    FILE *input_fp = fopen(filename, "r");
    printf("Reading %d bytes from %s.\n", (int)buf.st_size, filename);

    // "place" into directory
    directory[directory_entry].in_use = 1;
    directory[directory_entry].inode = inode_index;
    strncpy(directory[directory_entry].filename, base, strlen(base));

    inodes[inode_index].file_size = buf.st_size;
    inodes[inode_index].in_use = 1;
    memset(inodes[inode_index].blocks, -1, BLOCKS_PER_FILE * sizeof(int32_t));

    while (copy_size > 0)
    {
        fseek(input_fp, offset, SEEK_SET);

        // find a free block
        block_index = findFreeBlock();
        if (block_index == -1)
        {
            printf("ERROR: no free block found.\n");
            return;
        }

        int32_t bytes = fread(curr_image[block_index], BLOCK_SIZE, 1, input_fp);

        // save the block in the inode
        if (inode_block == BLOCKS_PER_FILE)
        {
        }
        inodes[inode_index].blocks[inode_block] = block_index;

        if (bytes == 0 && !feof(input_fp))
        {
            free_inodes[inode_index] = 1; // It is now free again
            printf("ERROR: An error occurred while trying to read from the input file.\n");
            return;
        }

        // clear the EOF flag
        clearerr(input_fp);

        // reduce copy_size by the BLOCK_SIZE bytes
        copy_size -= BLOCK_SIZE;

        // increment offset
        offset += BLOCK_SIZE;
    }
    fclose(input_fp);
}

void retrieve(char *tokens[MAX_NUM_ARGUMENTS])
{
    if (!image_open)
    {
        printf("insert: ERROR: Disk image not open.\n");
        return;
    }

    char *src = tokens[1];
    char *dst = tokens[2] ? tokens[2] : src;

    int32_t inode;

    if ((inode = find_file_by_name(src, NULL)) == -1)
    {
        fprintf(stderr, "retrieve: ERROR: File not found\n");
        return;
    }

    FILE *temp = fopen(dst, "w");

    if (!temp)
    {
        fprintf(stderr, "retrieve: Error: Could not open file `%s' for writing\n", dst);
        return;
    }

    struct inode this = inodes[inode];
    uint32_t rem = this.file_size;

    int i = 0;
    while (rem > 0 && this.blocks[i] != -1)
    {
        uint32_t to_copy = BLOCK_SIZE;

        if (rem < BLOCK_SIZE)
            to_copy = rem;

        assert(i < BLOCKS_PER_FILE);

        fwrite(curr_image[this.blocks[i++]], 1, to_copy, temp);

        rem -= BLOCK_SIZE;
    }

    fclose(temp);
}

// Read a file from virtual file system and output it to the terminal
// as a Hexadecimal value
void readfile(char *tokens[MAX_NUM_ARGUMENTS])
{
    if (!image_open)
    {
        printf("read: ERROR: Disk image not open.\n");
        return;
    }

    int32_t inode = find_file_by_name(tokens[1], NULL);
    if (inode == -1)
    {
        printf("read: ERROR: Can not find the file.\n");
        return;
    }

    struct inode this = inodes[inode];
    if (!this.file_size)
    {
        printf("read: File is empty\n");
        return;
    }

    uint32_t pos = atoi(tokens[2]);
    if (pos > this.file_size)
    {
        printf("read: file is only %d bytes", this.file_size);
        return;
    }

    int i = pos / BLOCK_SIZE;
    uint16_t offset = pos % BLOCK_SIZE;
    int32_t to_print = atoi(tokens[3]);

    // Help the user a bit since we don't really expect
    // them to know how long the file is
    if (to_print + pos > this.file_size)
        to_print = this.file_size - pos;

    bool remain = to_print % BLOCK_SIZE; // will the last line have its ascii not printed?

    int m = pos & 0xF;
    pos = pos & ~0xF;

    // Print the first header
    printf("%06X: ", pos);
    for (int j = 0; j < m; ++j)
        printf("-- ");

    // TODO:
    // 1. Change the entire thing to a three step process
    // 2. First line -> middle lines -> last line (if it has less than 16 bytes)
    // 3. Remove remain and offset variables

    while (to_print > 0 && i < BLOCKS_PER_FILE && this.blocks[i] != -1)
    {
        uint32_t end = to_print;

        if (end > BLOCK_SIZE)
            end = BLOCK_SIZE;

        uint8_t *this_blk = curr_image[this.blocks[i]];

        for (int j = offset; j < (end + offset); ++j)
        {
            // Some fancy printing
            printf("%02X ", this_blk[j]);

            // Print the ascii representation of the previous 16 bytes
            if ((j & 0xF) == 0xF)
            {
                int start = j - 15;
                if (offset > start)
                    start = offset;
                printf("  ");

                putchar('|');
                {
                    for (int k = start; k <= j; ++k)
                    {
                        char byte = this_blk[k];
                        if (byte >= 32 && byte < 127) // It is printable
                        {
                            putchar(byte);
                        }
                        else
                            putchar('.');
                    }
                }
                putchar('|');

                // print next line header
                pos += 16;
                printf("\n%06X: ", pos);
            }
        }

        offset = 0;
        to_print -= BLOCK_SIZE;

        ++i;
    }

    offset = pos % BLOCK_SIZE;
    if (remain)
    {
    }

    printf("\n");
}

// Delete a file from the file system using call 'delete
// An error occurs if a read-only file is marked for deletion
void del(char *tokens[MAX_NUM_ARGUMENTS])
{
    if (!image_open)
    {
        printf("ERROR: Disk image is not opened.\n");
        return;
    }

    // verify file exists
    uint8_t dir_idx;
    int inode_idx = find_file_by_name(tokens[1], &dir_idx);
    if (inode_idx == -1)
    {
        printf("delete: ERROR: Can not find the file.\n");
        return;
    }

    if (inodes[dir_idx].attribute & ATTRIB_R_ONLY)
    {
        printf("delete: ERROR: Can not delete read-only files.\n");
        return;
    }

    // set in use to false
    directory[inode_idx].in_use = 0;
    inodes[dir_idx].in_use = 0;

    // make space available again
    size_avail += inodes[dir_idx].file_size;

    // free each block in the file
    for (int i = 0; i < BLOCKS_PER_FILE; i++)
    {
        if (inodes[inode_idx].blocks[i] == -1)
            break;
        free_blocks[inodes[inode_idx].blocks[i]] = 1;
    }
}

// undelete a previously deleted file using call 'undel'
void undel(char *tokens[MAX_NUM_ARGUMENTS])
{
    if (!image_open)
    {
        printf("ERROR: Disk image is not opened.\n");
        return;
    }

    uint8_t dir_idx;
    uint32_t inode_idx = -1;

    int i = 0;
    for (; i < NUM_FILES; ++i)
    {
        if (!directory[i].in_use && !strncmp(tokens[1], directory[i].filename, 64))
        {
            inode_idx = directory[i].inode;
            break;
        }
    }

    if (inode_idx == -1)
    {
        printf("undelete: ERROR: Could not find the file.\n");
        return;
    }

    dir_idx = i;

    // set the file back to in-use
    directory[dir_idx].in_use = 1;
    inodes[inode_idx].in_use = 1;

    // reduce size_available by the file_size
    size_avail -= inodes[dir_idx].file_size;

    // remove requested file from undeleted blocks
    for (int i = 0; i < BLOCKS_PER_FILE; i++)
    {
        if (inodes[inode_idx].blocks[i] == -1)
            break;

        free_blocks[inodes[inode_idx].blocks[i]] = 0;
    }
}

void list(char *tokens[MAX_NUM_ARGUMENTS])
{
    if (!image_open)
    {
        printf("ERROR: Disk image is not opened.\n");
        return;
    }

    bool empty = true;
    bool list_hidden = false, list_attrib = false;

    // Parse options
    for (int i = 1; i < MAX_NUM_ARGUMENTS && tokens[i] != NULL; ++i)
    {
        if (*tokens[i] == '-')
        {
            char opt = tokens[i][1];
            switch (opt)
            {
            case 'h':
                list_hidden = true;
                break;
            case 'a':
                list_attrib = true;
                break;
            case '\0':
                fprintf(stderr, "list: ERROR: missing option parameter\n");
                break;
            default:
                fprintf(stderr, "list: unrecognized option %c\n", opt);
            }
        }
    }

    // Very inefficient, I suggest a linked-list structure instead
    for (int i = 0; i < NUM_FILES; ++i)
    {
        if (directory[i].in_use)
        {
            struct inode this = inodes[directory[i].inode];
            if ((this.attribute & ATTRIB_HIDDEN) && !list_hidden)
            {
                continue;
            }
            char temp[65];
            char *cpy = directory[i].filename;

            int j = 0;
            while (*cpy && j < 64)
            {
                temp[j++] = *(cpy++);
            }
            temp[j] = '\0';

            empty = false;
            if (list_attrib)
            {
                int spaces = 66 - strlen(temp);
                printf("%s%*hhu\n", temp, spaces, this.attribute);
            }
            else
                printf("%s\n", temp);
        }
    }

    if (empty)
    {
        printf("list: No files found.\n");
    }
}

// Outputs the amount of free space left on the disk image
// using global size variable
void df(char *tokens[MAX_NUM_ARGUMENTS])
{
    if (!image_open)
    {
        printf("ERROR: Disk Image not opened.\n");
        return;
    }

    printf("%d bytes free.\n", size_avail);
}

// opens a previously created file system
// reads whats currently in the image into the FILE* fp
// set the image to open
void openfs(char *tokens[MAX_NUM_ARGUMENTS])
{
    int max_size = sizeof(image_name) - 1;
    if (strlen(tokens[1]) > max_size)
    {
        fprintf(stderr, "Image name should not be longer than %d characters\n", max_size);
    }

    FILE *fp = fopen(tokens[1], "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Error opening file\n");
        return;
    }

    strncpy(image_name, tokens[1], max_size);
    int read = fread(&curr_image[0][0], BLOCK_SIZE, NUM_BLOCKS, fp);
    printf("Read %d blocks from %s\n", read, tokens[1]);

    image_open = 1;
    fclose(fp);
}

// closes disk image if it is open
void closefs(char *tokens[MAX_NUM_ARGUMENTS])
{
    if (image_open == 0)
    {
        printf("ERROR: Disk image not open\n");
        return;
    }

    image_open = 0;
    memset(image_name, 0, sizeof(image_name));
}

// create a new disk image and initialize it
void createfs(char *tokens[MAX_NUM_ARGUMENTS])
{
    int n = strlen(tokens[1]);
    if (n >= sizeof(image_name))
    {
        fprintf(stderr, "Image name too long");
    }

    // We do this "test run" to check if we can actually write a file
    // with this name in this directory so the user will not be stranded
    // later when calling `savefs`
    FILE *fp = fopen(tokens[1], "w");
    if (fp == NULL)
    {
        fprintf(stderr, "Failed to create file");
        return;
    }
    fclose(fp);

    init();
    strncpy(image_name, tokens[1], n);

    printf("File system image created!\n");
    image_open = 1;
}

// saves the disk image if one is currently open
void savefs(char *tokens[MAX_NUM_ARGUMENTS])
{
    if (image_open == 0)
    {
        printf("ERROR: Disk image is not open\n");
        return;
    }
    char *name = image_name;

    // If they provide a new name
    if (tokens[1] != NULL)
        name = tokens[1];

    FILE *fp = fopen(name, "w");
    if (fp == NULL)
        fprintf(stderr, "savefs: fatal error: could not open file for writing");

    int blocks_wrote = fwrite(curr_image, BLOCK_SIZE, NUM_BLOCKS, fp);

    if (blocks_wrote == 0 && !feof(fp))
        fprintf(stderr, "Error, could not write disk image to file\n");
    else
        printf("Wrote %d blocks to %s\n", blocks_wrote, name);

    fclose(fp);
}

// add and remove attributes to files in the disk image
// options are +h,+r to add the 'hidden' and 'read-only'
// attributes, respectively
//-h and -r are used to remove them
// Hidden files can only be seen when list -h is invoked
// Read only files can not be deleted
void attrib(char *tokens[MAX_NUM_ARGUMENTS])
{
    if (!image_open)
    {
        printf("Error: Disk Image not open.\n");
        return;
    }

    char *file = tokens[2];
    if (file == NULL)
    {
        printf("attrib: ERROR: File name was not read.\n");
        return;
    }
    uint32_t inode;

    if ((inode = find_file_by_name(file, NULL)) == -1)
    {
        fprintf(stderr, "attrib: File not found\n");
        return;
    }

    char flag = tokens[1][0];

    if (flag == '-' || flag == '+')
    {
        bool remove = (flag == '-');
        uint8_t mask = 0;

        char opt = tokens[1][1];

        switch (opt)
        {
        case 'h':
            mask = ATTRIB_HIDDEN;
            break;
        case 'r':
            mask = ATTRIB_R_ONLY;
            break;
        case '\0':
            fprintf(stderr, "list: ERROR: missing attribute parameter ('h' or 'r')\n");
            break;
        default:
            fprintf(stderr, "list: unrecognized attribute %c\n", opt);
        }

        if (remove)
        {
            inodes[inode].attribute &= ~mask;
        }
        else
        {
            inodes[inode].attribute |= mask;
        }
    }
    else
    {
        fprintf(stderr, "list: ERROR: `%s' is not an attribute. Expected attribute\n", tokens[1]);
        return;
    }
}

// Encrypt a file using a 256 bit cypher
void encrypt(char *tokens[MAX_NUM_ARGUMENTS])
{
    char *filename = tokens[1];
    uint8_t cipher = atoi(tokens[2]) & 0xFF;

    if (!image_open)
    {
        printf("Error: Disk Image not open.\n");
        return;
    }

    int32_t inode = find_file_by_name(filename, NULL);
    if (inode == -1)
    {
        fprintf(stderr, "encrypt: File not found\n");
        return;
    }

    xor_file(inode, cipher);
}

// Decrypt encypted cypher
void decrypt(char *tokens[MAX_NUM_ARGUMENTS])
{
    // Beauty of XOR ciphers
    encrypt(tokens);
}

// Initialize the disk image with starting parameters
// The directory takes the blocks 0-18
// Block 19 belongs to the free inodes map
// Blocks 20-277 are reserved for the inodes
// The rest of the blocks are free blocks to be used by the virtual file system
void init()
{
    directory = (struct directoryEntry *)&curr_image[0][0];
    inodes = (struct inode *)&curr_image[20][0];
    free_blocks = (uint8_t *)&curr_image[277][0];
    free_inodes = (uint8_t *)&curr_image[19][0];

    image_open = 0;
    memset(image_name, 0, 64);

    // Set the first 278 blocks to in_use since they are used for metadata
    memset(free_blocks, 0, FIRST_DATA_BLOCK);

    // Set the rest of the blocks to free since we just started
    for (int i = FIRST_DATA_BLOCK; i < NUM_BLOCKS; ++i)
        free_blocks[i] = 1;

    // This is the size of blocks 278-65535 in bytes
    size_avail = 66824192;

    for (int i = 0; i < NUM_FILES; ++i)
    {
        directory[i].in_use = 0;
        directory[i].inode = -1;
        free_inodes[i] = 1;
        memset(directory[i].filename, 0, 64);

        for (int j = 0; j < BLOCKS_PER_FILE; ++j)
        {
            inodes[i].blocks[j] = -1;
        }
        inodes[i].in_use = 0;
        inodes[i].attribute = 0;
        inodes[i].file_size = 0;
    }
}

void free_array(char **arr, size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        if (arr[i] != NULL)
        {
            free(arr[i]);
        }
    }
}

void parse_tokens(const char *command_string, char **token)
{
#define WHITESPACE " \t\n"

    // Clean up the old values in token
    free_array(token, MAX_NUM_ARGUMENTS);

    int token_count = 0;
    // Pointer to point to the token parsed by strsep
    char *argument_ptr = NULL;

    char *working_string = strdup(command_string);

    // we are going to move the working_string pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *head_ptr = working_string;

    // Tokenize the input strings with whitespace used as the delimiter
    while (((argument_ptr = strsep(&working_string, WHITESPACE)) != NULL) &&
           (token_count < MAX_NUM_ARGUMENTS))
    {
        token[token_count] = strndup(argument_ptr, MAX_COMMAND_SIZE);
        if (strlen(token[token_count]) == 0)
        {
            free(token[token_count]);
            token[token_count] = NULL;
        }
        token_count++;
    }

    // Set all args from the last arg parsed to the end of the token array
    // This helps so that I don't get a double free error if I run a cmd that
    // has 7 args then run one that has 3 args then called free_array on token
    for (; token_count < MAX_NUM_ARGUMENTS; ++token_count)
    {
        token[token_count] = NULL;
    }

    free(head_ptr);
}

int main(int argc, char **argv)
{
    char *command_string = (char *)malloc(MAX_COMMAND_SIZE);
    char *tokens[MAX_NUM_ARGUMENTS] = {NULL};

    init();

    while (1)
    {
        // Print out the msh prompt
        printf("mfs> ");

        // Read the command from the commandline.  The
        // maximum command that will be read is MAX_COMMAND_SIZE
        // This while command will wait here until the user
        // inputs something since fgets returns NULL when there
        // is no input
        while (!fgets(command_string, MAX_COMMAND_SIZE, stdin))
            ;

        // Ignore blank lines
        if (*command_string == '\n' || command_string[0] == ' ')
        {
            continue;
        }

        parse_tokens(command_string, tokens);

        char *cmd = tokens[0];

        // Quit if command is 'quit' or 'exit'
        if (!strcmp(cmd, "quit") || !strcmp(cmd, "exit"))
        {
            break;
        }

        int i;
        for (i = 0; i < NUM_COMMANDS; ++i)
        {
            if (!strcmp(cmd, commands[i].name))
            {
                if (tokens[commands[i].num_args] == NULL)
                {
                    fprintf(stderr, "%s: Not enough arguments\n", cmd);
                    break;
                }
                commands[i].run(tokens);
                break;
            }
        }

        if (i == NUM_COMMANDS)
        {
            fprintf(stderr, "mfs: Invalid command `%s'\n", cmd);
        }
    }

    free(command_string);
    free_array(tokens, MAX_NUM_ARGUMENTS);
    return 0;
}
