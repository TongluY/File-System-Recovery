#include "ext2_fs.h"
#include "read_ext2.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>

void process_block(int fd, struct ext2_inode inode, int block_num, int *blocks_left, int indirection, char *output_dir, int inum)
{
    if (indirection == 0)
    {
        // base case
        char buffer[1024];
        lseek(fd, BLOCK_OFFSET(block_num), SEEK_SET);
        read(fd, &buffer, 1024);
        char output_file[256];
        sprintf(output_file, "%s/file-%d.jpg", output_dir, inum);
        FILE *fp = fopen(output_file, "wb");
        if (fp == NULL)
        {
            printf("Failed to create file %s\n", output_file);
            exit(1);
        }
        fwrite(buffer, sizeof(char), 1024, fp);
        fclose(fp);
        (*blocks_left)--;
    }
    else
    {
        // recursive case
        char buffer[1024];
        lseek(fd, BLOCK_OFFSET(block_num), SEEK_SET);
        read(fd, &buffer, 1024);
        int *block_array = (int *)buffer;
        for (int i = 0; i < 256 && *blocks_left > 0; i++)
        {
            // printf("block id before if: %d", block_array[i]);
            if (block_array[i] != 0)
            {
                process_block(fd, inode, block_array[i], blocks_left, indirection - 1, output_dir, inum);
                // printf("block id: %d", block_array[i]);
            }
          
        }
    }
    // printf("blocks left: %d \n", *blocks_left);
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("expected usage: ./runscan inputfile outputfile\n");
        exit(0);
    }

    /* This is some boilerplate code to help you get started, feel free to modify
       as needed! */

    int fd;
    fd = open(argv[1], O_RDONLY); /* open disk image */

    ext2_read_init(fd);

    struct ext2_super_block super;
    struct ext2_group_desc group;

    // example read first the super-block and group-descriptor
    read_super_block(fd, 0, &super);

    char *output_dir = argv[2];
    mkdir(output_dir, 0777);
    for (unsigned int i = 0; i < num_groups; ++i) // num_groups
    {
        read_group_desc(fd, i, &group);
        off_t inode_table_start = locate_inode_table(i, &group);

        // Scan all inodes that represent regular files and
        for (unsigned int j = 0; j < inodes_per_group; ++j) // inodes_per_group
        {
            // j+1+i*inodes_per_group
            struct ext2_inode *inode = (struct ext2_inode *)malloc(sizeof(struct ext2_inode));
            read_inode(fd, inode_table_start, j + 1, inode, 256);
            if (S_ISREG(inode->i_mode))
            {
                char buffer[1024];
                lseek(fd, BLOCK_OFFSET(inode->i_block[0]), SEEK_SET);
                read(fd, &buffer, 1024); // &buffer
                if (buffer[0] == (char)0xff &&
                    buffer[1] == (char)0xd8 &&
                    buffer[2] == (char)0xff &&
                    (buffer[3] == (char)0xe0 ||
                     buffer[3] == (char)0xe1 ||
                     buffer[3] == (char)0xe8))
                {
                    int blocks_left = (inode->i_size / 1024) + ((inode->i_size % 1024) != 0);
                    for (int k = 0; k < 12; k++)
                    {
                        if (inode->i_block[k] != 0)
                        {
                            process_block(fd, *inode, inode->i_block[k], &blocks_left, 0, output_dir, j + 1);
                            // printf("blocks_left: %d \n", blocks_left);
                        }
                        if (inode->i_block[12] != 0)
                        {
                            // printf("blocks left before calling process_block : %d \n", blocks_left);
                            process_block(fd, *inode, inode->i_block[12], &blocks_left, 1, output_dir, j + 1);
                            if (inode->i_block[13] != 0)
                            {
                                process_block(fd, *inode, inode->i_block[13], &blocks_left, 2, output_dir, j + 1);
                                if (inode->i_block[14] != 0)
                                {
                                    process_block(fd, *inode, inode->i_block[14], &blocks_left, 3, output_dir, j + 1);
                                }
                            }
                        }
                    }
                }
            }
        }

        return 0;
    }
}