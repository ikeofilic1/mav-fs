#define _GNU_SOURCE 1

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define BLOCK_SIZE 1024
#define BLOCKS_PER_FILE 1024

#define MAX_FILE_LEN 64
#define NUM_FILES 256

#define DISK_IMAGE_SIZE 67108864
#define NUM_BLOCKS (DISK_IMAGE_SIZE) / (BLOCK_SIZE)

// No command has more than 5 arguments in our
// list of commands
#define MAX_NUM_ARGUMENTS 5
#define MAX_COMMAND_SIZE 255

uint8_t curr_image[NUM_BLOCKS][BLOCK_SIZE];

struct directoryEntry
{
    char filename[64];
    bool in_use;
    int32_t inode;
};

struct directoryEntry *directory;
struct inode *inodes;

struct inode
{
    int32_t blocks[BLOCKS_PER_FILE];
    bool in_use;
    uint8_t attrib;
};

// Command stuff
typedef void (*command_fn)(char *[MAX_NUM_ARGUMENTS]);

typedef struct _command
{
    char *name;
    command_fn run;
    uint8_t num_args;
} command;

void insert(char *tokens[MAX_NUM_ARGUMENTS])
{
}
void retrieve(char *tokens[MAX_NUM_ARGUMENTS])
{
}
void readfile(char *tokens[MAX_NUM_ARGUMENTS])
{
    if (tokens[1] == NULL || tokens[2] == NULL || tokens[3] == NULL)
    {
        fprintf(stderr, "Not enough parameters\n");
    }

    // TODO: I changed the name to match what it actually does
    // You have to find the file in the file_system and then print the contents 
    // out in hex

}
void del(char *tokens[MAX_NUM_ARGUMENTS])
{
}
void undel(char *tokens[MAX_NUM_ARGUMENTS])
{
}
void list(char *tokens[MAX_NUM_ARGUMENTS])
{
    
}
void df(char *tokens[MAX_NUM_ARGUMENTS])
{
}
void openfs(char *tokens[MAX_NUM_ARGUMENTS])
{
    if (tokens[1] == NULL)
    {
        fprintf(stderr, "Filename not provided\n");
        return;
    }

    char full_path[256];
    if (tokens[1][0] == '/')
    {
        strncpy(full_path, tokens[1], sizeof(full_path));
    }
    else
    {
        char *cwd = getcwd(NULL, 0);
        snprintf(full_path, sizeof(full_path), "%s/%s", cwd, tokens[1]);
        free(cwd);
    }

    FILE *fs_file = fopen(full_path, "r");
    if (fs_file == NULL)
    {
        fprintf(stderr, "File not found\n");
        return;
    }
    // TODO: You have to read the file into curr_image. You can just call fread
    // with sizeof data and the filename. Basically just read sizeof(data) bytes

    fclose(fs_file);
}
void closefs(char *tokens[MAX_NUM_ARGUMENTS])
{
}
void createfs(char *tokens[MAX_NUM_ARGUMENTS])
{
    if (tokens[1] == NULL)
    {
        fprintf(stderr, "Filename not provided\n");
        return;
    }

    FILE *fs_file = fopen(tokens[1], "w");
    if (fs_file == NULL)
    {
        fprintf(stderr, "Failed to create file");
        return;
    }
    fclose(fs_file);
    printf("File system image created!\n");
    if (tokens[1] == NULL)
    {
        fprintf(stderr, "Filename not provided\n");
        return;
    }

    FILE *fs_file = fopen(tokens[1], "w");
    if (fs_file == NULL)
    {
        fprintf(stderr, "Failed to create file");
        return;
    }
    fclose(fs_file);
    printf("File system image created!\n");

    // TODO: call init to reinitialize the file system as new
}
void savefs(char *tokens[MAX_NUM_ARGUMENTS])
{
}
void attrib(char *tokens[MAX_NUM_ARGUMENTS])
{
}
void encrypt(char *tokens[MAX_NUM_ARGUMENTS])
{
}
void decrypt(char *tokens[MAX_NUM_ARGUMENTS])
{
}

// As of now, we only have 14 commands
#define NUM_COMMANDS 14

// We use a table to store and lookup command names and their corresponding functions.
// Essentially, this is a map/dictionary that is highly modular (compared to a massive\
// switch statement). Also, if the table decides to grow larger, we can always store the
// elements in order of their keys (the name field), and use binary search when looking up a
// command as opposed to linearly scanning the commands table

static const command commands[NUM_COMMANDS] = {
    //  command name	call back	min arguments

    {"insert", insert, 1},
    {"retrieve", retrieve, 2},
    {"read", readfile, 3},
    {"delete", del, 1},
    {"undel", undel, 1},
    {"list", list, 0},
    {"df", df, 0},
    {"open", openfs, 1},
    {"close", closefs, 0},
    {"createfs", createfs, 1},
    {"savefs", savefs, 0},
    {"attrib", attrib, 1},
    {"encrypt", encrypt, 2},
    {"decrypt", decrypt, 2},
};

///////////////////////////////////////
// Forward declarations
//////////////////////////////////////
void parse_tokens(const char *command_string, char **token);
void free_array(char **arr, size_t size);

void init(void)
{
    directory = (struct directoryEntry *)&curr_image[0][0];
    inodes = (struct inode *)&curr_image[20][0];

    for (int i = 0; i < NUM_FILES; ++i)
    {
        directory[i].in_use = 0;
        directory[i].inode = -1;
        for (int j = 0; j < BLOCKS_PER_FILE; ++j)
        {
            inodes[i].blocks[j] = -1;
        }
        inodes[i].in_use = 0;
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
        if (*command_string == '\n')
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
