 /* 
 *  mkfs.sfs.c
 *  created by Yubin Ruan, 2016 <my email...>
 *
 * This file is part of the sfs filesystem source code, which is targeted at
 * Linux kernel version 3.1x-4.6x. All of the source code are licensed under  
 * the Creative Commons Zero License, a public domain license. You can
 * redistribute it or modify in any way you want. It is distributed in the hope
 * that it will be useful and educational for learning and hacking the Linux
 * kernel, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */
   
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>

#include "sfs.h"

void usage() {
    fprintf(stderr, "\nusage: mkfs.sfs /path/to/device(or file\n\n");
}

/* 
 * NOTE: we haven't consider any endianess thing yet !
 */

int main(int argc, char *argv[])
{
    struct sfs_sb_info si = {
        .magic          = SFS_MAGIC_NUMBER,
        .version        = 0x1,
        .blk_size       = SFS_BLK_SIZE,
        .sfs_ino_bitmap = SFS_SB_START_NR+1,
        .sfs_blk_bitmap = SFS_SB_START_NR+2,
        .sfs_ino_start  = SFS_SB_START_NR+3,
        /* this sfs_blk_start refer to all the blk(including the boot sector) */
        .sfs_blk_start  = 0,
    };

    struct sfs_inode_info ri = {
        .mode            = S_IFDIR,
        .inode_no        = SFS_ROOTINO,
        .slot_nr         = SFS_ROOT_SLOT_NR,
        .directs         = {0},
        .indirect        = 0,
        .file_size        = SFS_BLK_SIZE,
    };

    char buffer[SFS_BLK_SIZE * 200] = {'\0'};
    int nbyte;
    FILE *fh;

    if (argc != 2) {
        usage();
        return -1;
    }

    fh = fopen(argv[1], "w+");
    if (!fh) {
        perror("Cannot open file");
        return -1;
    }

    fseek(fh, 0, SEEK_SET);
    nbyte = fwrite(buffer, 1, SFS_BLK_SIZE, fh);
    if (nbyte != SFS_BLK_SIZE) {
        fprintf(stderr, "fail to write boot sector \n");
        return -1;
    }

    fseek(fh, SFS_BLK_SIZE, SEEK_SET);
    nbyte = fwrite(&si, 1, sizeof(struct sfs_sb_info), fh);
    if (nbyte != sizeof(struct sfs_sb_info)) {
        fprintf(stderr,
             "fail to completely write super block!! nbyte:[%d]\n", nbyte);
        return -1;
    }

    fseek(fh, SFS_BLK_SIZE*2, SEEK_SET);
    buffer[0] = 0x1;
    nbyte = fwrite(buffer, 1, 1, fh);
    if (nbyte != 1) {
        fprintf(stderr,
          "fail to completely write inode_bitmap block!! nbyte:[%d]\n", nbyte);
        return -1;
    }

    fseek(fh, SFS_BLK_SIZE*3, SEEK_SET);
    buffer[0]=63;
    nbyte = fwrite(buffer, 1, 1, fh);
    if (nbyte != 1) {
        fprintf(stderr, 
           "fail to completely write blk_bitmap block!! nbyte:[%d]\n", nbyte);
        return -1;
    }
    buffer[0]=0;

    fseek(fh, SFS_BLK_SIZE*4, SEEK_SET);
    /* preallocate one block for all inode, and 100 block to hold data */
    nbyte = fwrite(buffer, 1, SFS_BLK_SIZE * 101, fh);
    if (nbyte != SFS_BLK_SIZE * 101) {
        fprintf(stderr,
             "fail to prealloc blocks! nbyte:[%d]\n", nbyte);
        return -1;
    }

    fseek(fh, SFS_BLK_SIZE*4, SEEK_SET);
    nbyte = fwrite(&ri, 1, sizeof(struct sfs_inode_info), fh);
    if (nbyte != sizeof(struct sfs_inode_info)) {
        fprintf(stderr,
             "fail to completely write prealloc inode block!! nbyte:[%d]\n", nbyte);
        return -1;
    }


    printf("\nsuccessfully written all thing.");
    printf("magic number:[0x%lx], blk_size:[0x%lx] sfs version:[%ld]\n\n",
           si.magic, si.blk_size, si.version);
    return 0;
}
