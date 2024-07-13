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
// ./runscan ../updated_test_disk_images/test_3/03.img test3
void process_block(int fd, struct ext2_inode inode, int block_num, int *blocks_left, int *extra, int indirection, char *output_dir, int inum, FILE *fp)
{

    if (indirection == 0)
    {
        // base case
        if(*blocks_left == 1 && *extra >0){
            char buffer[*extra];
            lseek(fd, BLOCK_OFFSET(block_num), SEEK_SET);
            read(fd, &buffer, *extra);
            fwrite(buffer, sizeof(char), *extra, fp);
            blocks_left = 0;
            return;
        }
        if(*blocks_left == 0){return;}
        char buffer[1024];
        lseek(fd, BLOCK_OFFSET(block_num), SEEK_SET);
        read(fd, &buffer, 1024);
        fwrite(buffer, sizeof(char), 1024, fp);
        (*blocks_left)--;
    }
    //python3 rcheck.py <your output dir> <test output dir>
    else
    {
        // recursive case
        char buffer[1024];
        lseek(fd, BLOCK_OFFSET(block_num), SEEK_SET);
        read(fd, &buffer, 1024);
        int *block_array = (int *)buffer;
        for (int i = 0; i < 256 && *blocks_left > 0; i++)
        {
            if (block_array[i] != 0)
            {
                process_block(fd, inode, block_array[i], blocks_left, extra, indirection - 1, output_dir, inum, fp);
            }
            else
                break;
        }
    }
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("expected usage: ./runscan inputfile outputfile\n");
        exit(0);
    }

    int fd;
    fd = open(argv[1], O_RDONLY); /* open disk image */

    ext2_read_init(fd);

    struct ext2_super_block super;
    struct ext2_group_desc group;

    char *output_dir = argv[2];
    if(opendir(output_dir) != NULL){
        printf("ERROR: Directory already exists!\n");
        exit(1);
    }
    mkdir(output_dir, 0777);

    // example read first the super-block and group-descriptor
    read_super_block(fd, 0, &super); 
    read_group_desc(fd, 0, &group);
    //read_group_desc(fd, 0, &group);//maybe comment out


    for (unsigned int i = 0; i < num_groups; ++i) // num_groups
    {
         //maybe change to i
        off_t inode_table_start = locate_inode_table(i, &group);
        // read_group_desc(fd, i, &group);
        // Scan all inodes that represent regular files and
        for (unsigned int j = 0; j < inodes_per_group; ++j) // inodes_per_group
        {
            struct ext2_inode inode;
            read_inode(fd, inode_table_start, j + 1, &inode, 2 * sizeof(inode));

            if (S_ISREG(inode.i_mode))
            {
                char buffer[1024];
                lseek(fd, BLOCK_OFFSET(inode.i_block[0]), SEEK_SET);
                read(fd, &buffer, 1024); // &buffer
                if (buffer[0] == (char)0xff &&
                    buffer[1] == (char)0xd8 &&
                    buffer[2] == (char)0xff &&
                    (buffer[3] == (char)0xe0 ||
                     buffer[3] == (char)0xe1 ||
                     buffer[3] == (char)0xe8))
                {
                    char output_file[256];
                    sprintf(output_file, "%s/file-%d.jpg", output_dir, j + 1 + i * inodes_per_group);
                    FILE *fp = fopen(output_file, "wb");
                    if (fp == NULL)
                    {
                        printf("Failed to create file %s\n", output_file);
                        exit(1);
                    }

                    char details[256];
                    sprintf(details, "%s/file-%d-details.txt", output_dir, j + 1 + i * inodes_per_group);
                    FILE *detailtxt = fopen(details, "w");
                    fprintf(detailtxt, "%d\n", inode.i_links_count);
                    fprintf(detailtxt, "%d\n", inode.i_size);
                    fprintf(detailtxt, "%d", inode.i_uid);
                    fclose(detailtxt);
                    int blocks_left = inode.i_size / 1024;
                    int extra = inode.i_size % 1024;
                    if(extra>0) blocks_left++;
                    for (int k = 0; k < 12; k++)
                    {
                            process_block(fd, inode, inode.i_block[k], &blocks_left, &extra, 0, output_dir, j + 1 + i * inodes_per_group, fp);
                    }
                    if (inode.i_block[12] != 0)
                    {
                        process_block(fd, inode, inode.i_block[12], &blocks_left, &extra, 1, output_dir, j + 1 + i * inodes_per_group, fp);
                        if (inode.i_block[13] != 0)
                        {
                            process_block(fd, inode, inode.i_block[13], &blocks_left,&extra,2, output_dir, j + 1 + i * inodes_per_group, fp);
                        }
                    }

                    
                    fclose(fp);
                }
            }
        }
        // top secret
        // test5/test6 - top secret && multiple groups
        for (unsigned int j = 0; j < inodes_per_group; ++j) // inodes_per_group
        {
            struct ext2_inode inode;
            read_inode(fd, inode_table_start, j + 1, &inode, 2 * sizeof(inode));
            if (S_ISDIR(inode.i_mode))
            {
                char directoryBuffer[1024];
                lseek(fd, BLOCK_OFFSET(inode.i_block[0]), SEEK_SET);
                read(fd, &directoryBuffer, 1024);
                int offset = 0;
                struct ext2_dir_entry *dentry = (struct ext2_dir_entry *)&(directoryBuffer[offset]);
                int name_len = dentry->name_len & 0xFF;
                for (unsigned int x = 0; x < inode.i_block[0]; x++)
                {
                    dentry = (struct ext2_dir_entry *)&(directoryBuffer[offset]);
                    name_len = dentry->name_len & 0xFF;
                    if (name_len != 0)
                    {
                        char name[EXT2_NAME_LEN];
                        strncpy(name, dentry->name, name_len);

                        name[name_len] = '\0';
                        
                        if (name_len % 4 != 0)
                            offset += 8 + name_len + (4 - (name_len % 4));
                        else
                            offset += 8 + name_len;

                        char filePath[256];
                        sprintf(filePath, "%s/file-%i.jpg", output_dir, dentry->inode);
                        FILE *inodeFileName;
                        inodeFileName = fopen(filePath, "r");
                        if (inodeFileName == NULL)
                            continue;
                        else
                        {
                            char copyBuffer[1024];
                            char nameWithPath[256];
                            sprintf(nameWithPath, "%s/%s", output_dir, name);
                            FILE *actualName = fopen(nameWithPath, "w");
                            while (!feof(inodeFileName))
                            {
                                int length = fread(copyBuffer, 1, 1024, inodeFileName);
                                fwrite(copyBuffer, 1, length, actualName);
                            }
                            fclose(inodeFileName);
                            fclose(actualName);                                              
                        }
                        
                    }
                }
            }
        }
    }

    return 0;
}
