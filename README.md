# Mav-fs

This is a user-space portable index-allocated file system written entirely in C. The file system provides 2<sup>26</sup> bytes of drive space in a disk image. Users can create the filesystem image, list the files currently in the file system, add and remove files, and save the filesystem. Files will persist in the file system image when the program exits (as long as the user saves before exiting).

## Features / User Guide
1. The program prints out a prompt of "mfs>" when it is ready to accept input.
2. The following commands are implemented:

|Command|Usage|Description|
|-------|-----|-----------|
|insert|```insert <filename>```|Copy the file into the filesystem image|
|retrieve|```retrieve <filename>```|Retrieve the file from the filesystem image and place it in the current working directory|
|retrieve|```retrieve <filename> <newfilename>```|Retrieve the file from the filesystem image and place it in the current working directory using the new filename|
|read|```read <filename> <starting byte> <number of bytes>```|Print \<number of bytes\> bytes from the file, in hexadecimal, starting at \<starting byte\>
|delete|```delete <filename>```|Delete the file from the filesystem image|
|undel|```undelete <filename>```|Undelete the file from the filesystem image|
|list|```list [-h] [-a]```|List the files in the filesystem image. If the ```-h``` parameter is given it will also list hidden files. If the ```-a``` parameter is provided the attributes will also be listed with the file and displayed as an 8-bit binary value.|
|df|```df```|Display the amount of disk space left in the filesystem image|
|open|```open <filename>```|Open a filesystem image|
|close|```close```|Close the opened filesystem image|
|createfs|```createfs <filename>```|Creates a new filesystem image|
|savefs|```savefs```|Write the currently opened filesystem to its file|
|attrib|```attrib [+attribute] [-attribute] <filename>```|Set or remove the attribute for the file|
|encrypt|```encrypt <filename> <cipher>```|XOR encrypt the file using the given cipher.  The cipher is limited to a 1-byte value|
|decrypt|```encrypt <filename> <cipher>```|XOR decrypt the file using the given cipher.  The cipher is limited to a 1-byte value|
|quit|```quit```|Quit the application|

3. The filesystem uses an index allocation scheme.
4. The filesystem has 65536 blocks with a block size of 1024 bytes.
5. The filesystem supports files up to 2<sup>20</sup> bytes in size.
6. The filesystem supports up to 256 files.
7. The filesystem supports filenames of up to 64 alphanumeric characters including the optional extension.
8. The directory structure is a single-level hierarchy with no subdirectories stored in blocks 0-18.
8. The filesystem allocates block 19 for the free inode map
10. The filesystem allocates blocks 20-276 for inodes
11. The filesystem allocates block 277 for the free block map
12. Blocks 278-65535 are used for file data.
13. Files are not required to be contiguous. Blocks do not have to be sequential.

## Command Details

### ```insert``` 

```insert``` allows the user to put a new file into the file system.
 
The command takes the form:

```insert <filename>```

If the filename is too long, an error is returned stating:

```insert error: File name too long.```

If there is not enough disk space for the file, an error is returned stating:

```insert error: Not enough disk space.```

### ```retrieve``` 

The ```retrieve``` command allows the user to retrieve a file from the file system and place it in the current working directory.

The command takes the form:

```retrieve <filename>```

and

```retrieve <filename> <newfilename>```

If no new filename is specified, the ```retrieve``` command copies the file to the current working directory using the filename from the file system.

If the file does not exist in the file system, an error is printed that states: 

```Error: File not found.```

### ```delete``` command

The ```delete``` command allows the user to delete a file from the file system

If the file exists in the file system, it is marked as deleted (free) and the space is available for new files.

### ```undelete``` command

The ```undelete``` command allows the user to undelete a file that has been deleted from the file system

If the file exists in the file system directory AND is marked deleted, it shall be undeleted.

If the file is not found in the directory then the following is printed:

```undelete: Can not find the file.```

### ```list``` command 

The ```list``` command displays all the files in the file system and their size in bytes

If no files are in the file system, a message is printed: 

```list: No files found.```

Note that files that are marked as hidden are not listed

### ```df``` command

The ```df``` command displays the amount of free space in the file system in bytes.

### ```open``` command

The ```open``` command opens a file system image file with the name and path given by the user.

If the file is not found a message shall be printed:

```open: File not found```

### ```close``` command

The ```close``` command closes a file system image file with the name and path given by the user.

If the file is not open, a message is printed:

```close: File not open```

### ```savefs command```

The ```savefs``` command writes the file system to disk.

### ```attrib``` command

The ```attrib``` command sets or removes an attribute from the file.

Valid attributes are:

|Attribute|Description|
|---------|-----------|
| h       | Hidden. The file does not display in the directory listing|
| r       | Read-Only. The file is marked read-only and can not be deleted.|


To set the attribute on the file the attribute tag is given with a +, ex:

```mfs> attrib +h foo.txt```

To remove the attribute on the file the attribute tag is given with a -, ex:

```mfs> attrib -h foo.txt```

If the file is not found, a message is printed:

```attrib: File not found```

```createfs command```

### ```createfs``` command 

```createfs``` creates a file system image file with the name provided by the user.

If the file name is not provided, a message is printed:

```createfs: Filename not provided```

### ```encrypt``` command 

The ```encrypt``` command allows the user to encrypt a file in the file system using the provided cipher.  This is a simple byte-by-byte [XOR cipher](https://en.wikipedia.org/wiki/XOR_cipher).

The command takes the form:

```encrypt <filename> <cipher>```

The cipher is required to be 256 bits.

### ```decrypt``` command 

The ```decrypt``` command allows the user to decrypt a file in the file system using the provided cipher.  This is a simple byte-by-byte [XOR cipher](https://en.wikipedia.org/wiki/XOR_cipher). 

The command takes the form:

```decrypt <filename> <cipher>```

The cipher is required to be 256 bits.
