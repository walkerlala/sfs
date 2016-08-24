/* 
 *  fs/sfs/super.c
 *  created by Yubin Ruan, 2016 <my email...>
 *
 *  Inspired by:
 *     fs/ufs/super.c
 *
 * This file is part of the sfs filesystem source code, which is targeted at
 * Linux kernel version 3.1x-4.6x. All of the source code are licensed under  
 * the Creative Commons Zero License, a public domain license. You can
 * redistribute it or modify in any way you want. It is distributed in the hope
 * that it will be useful and educational for learning and hacking the Linux
 * kernel, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/module.h> /* module stuff */
#include <asm/uaccess.h>  /* use-kernel-space access */
#include <linux/errno.h>
#include <linux/fs.h>          /* definition of some VFS structs*/
#include <linux/blkdev.h>      /* request_queue and request */
#include <linux/buffer_head.h> /* struct buffer_head, sb_bread() */
#include <linux/statfs.h>      /* struct kstatfs */
#include <linux/mount.h>       /* struct vfsmount */
#include <linux/version.h>

#include "sfs.h"

static struct kmem_cache *sfs_inode_cachep;

/*
 * FIXME file_name now are fixed length long: 14 char at most
 */

/*============= helper function =====================*/

/* get sfs_sb_info out of a *sb */
static inline struct sfs_sb_info *SFS_S_INFO(struct super_block *sb) {
    return sb->s_fs_info;
}

/* get sfs_inode_info out from a *inode */
static inline struct sfs_inode_info *SFS_I_INFO(struct inode *inode) {
    return inode->i_private;
}

/* you have to update the inode bit map yourself */
int __sfs_get_next_inode_nr(struct super_block *sb) {
    struct buffer_head *bh;
    char *raw_data;
    int i, j, ret = -EINVAL;

    bh = sb_bread(sb, SFS_S_INFO(sb)->sfs_ino_bitmap);
    if (unlikely(!bh)) {
        SFSD(SFS_KERN_LEVEL "FAIL sb_read() 2 !!\n");
        return -ENOMEM;
    }
    raw_data = bh->b_data;
    for (i = 0; i < bh->b_size; i++) {
        for (j = 0; j < 8; j++) {
            if (!(*(raw_data + i) & (1 << j)))
                goto found_bit;
        }
    }
found_bit:
    if (!(i == bh->b_size && j == 8)) {
        printk(SFS_KERN_LEVEL "find inode:[%d]\n", 8 * i + j);
        ret = 8 * i + j;
    }
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    return ret;
}

/* you have to update the blk bit map yourself */
unsigned int __sfs_get_unused_blk(struct super_block *sb) {
    struct buffer_head *bh;
    char *raw_data;
    int i, j, ret = 0;

    bh = sb_bread(sb, SFS_S_INFO(sb)->sfs_blk_bitmap);
    if (unlikely(!bh)) {
        SFSD(SFS_KERN_LEVEL "FAIL sb_read() 3 !\n");
        return 0;
    }
    raw_data = bh->b_data;
    for (i = 0; i < bh->b_size; i++) {
        for (j = 0; j < 8; j++) {
            if (!(*(raw_data + i) & (1 << j)))
                goto found_bit;
        }
    }
found_bit:
    if (!(i == bh->b_size && j == 8)) {
        printk(SFS_KERN_LEVEL "find unused blk:[%d]\n", 8 * i + j);
        ret = 8 * i + j;
    }
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    return ret;
}

int sfs_update_prealloc_inodes(struct super_block *sb,
                               struct sfs_inode_info *sii) {
    struct buffer_head *bh;
    struct sfs_inode_info *tmp_sii;

    bh = sb_bread(sb, SFS_S_INFO(sb)->sfs_ino_start);
    if (unlikely(!bh)) {
        SFSD(SFS_KERN_LEVEL "fail sb_bread()!!\n");
        return -ENOMEM;
    }

    tmp_sii = (struct sfs_inode_info *)bh->b_data;
    tmp_sii += sii->inode_no;
    memcpy(tmp_sii, sii, sizeof(struct sfs_inode_info));

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    return 0;
}

struct sfs_inode_info *sfs_get_inode(struct super_block *sb, uint64_t ino) {
    struct buffer_head *bh;
    struct sfs_inode_info *sii;

    bh = sb_bread(sb, SFS_S_INFO(sb)->sfs_ino_start);
    if (unlikely(!bh)) {
        SFSD(SFS_KERN_LEVEL "FAIL sb_bread()\n");
        return NULL;
    }

    sii = (struct sfs_inode_info *)kmem_cache_alloc(sfs_inode_cachep, GFP_KERNEL);
    if (!sii) {
        SFSD(SFS_KERN_LEVEL "FAIL kmem_cache_alloc()!\n");
        return NULL;
    }
    memcpy(sii, &(((struct sfs_inode_info *)(bh->b_data))[ino]),
           sizeof(struct sfs_inode_info));

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    return sii;
}

/* test whether a bit is set. result should be 0 or 1. otherwise error */
int sfs_test_blk_bmp_bit(struct super_block *sb, uint64_t blk_nr) {
    struct buffer_head *bh;
    char *raw_data;
    int err = -EINVAL;

    bh = sb_bread(sb, SFS_S_INFO(sb)->sfs_blk_bitmap);
    if (unlikely(!bh)) {
        SFSD(SFS_KERN_LEVEL "FAIL sb_bread() !!!\n");
        goto release;
    }

    raw_data = bh->b_data;
    if (blk_nr / 8 >= bh->b_size) {
        printk(SFS_KERN_LEVEL "too large blk_nr to test:[%llu]. aborted.\n",
                blk_nr);
        goto release;
    }
    raw_data += blk_nr / 8;
    if (*raw_data & 1 << (blk_nr % 8)) {
        SFSD(SFS_KERN_LEVEL "SUCCESS blk_nr test !!!\n");
        err = 0;
    }
release:
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    return err;
}

/* test whether a bit is set. result should be 0 or 1. otherwise error */
int sfs_test_ino_bmp_bit(struct super_block *sb, uint64_t ino_nr) {
    struct buffer_head *bh;
    char *raw_data;
    int err = -EINVAL;

    bh = sb_bread(sb, SFS_S_INFO(sb)->sfs_ino_bitmap);
    if (unlikely(!bh)) {
        SFSD(SFS_KERN_LEVEL "fail sb_bread() !!!\n");
        goto release;
    }

    raw_data = bh->b_data;
    if (ino_nr / 8 >= bh->b_size) {
        printk(SFS_KERN_LEVEL "too large ino_nr to test:[%llu]. aborted.\n",
               ino_nr);
        goto release;
    }
    raw_data += ino_nr / 8;
    if (*raw_data & 1 << (ino_nr % 8)) {
        printk(SFS_KERN_LEVEL "ino_nr set.\n");
        err = 0;
    }
release:
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    return err;
}

/* update bitmap. 0 on success */
int sfs_update_blk_bmp_bit(struct super_block *sb, uint64_t blk_nr) {
    struct buffer_head *bh;
    char *raw_data;
    int err = -EINVAL;

    bh = sb_bread(sb, SFS_S_INFO(sb)->sfs_blk_bitmap);
    if (unlikely(!bh)) {
        SFSD(SFS_KERN_LEVEL "FAIL sb_bread() !!\n");
        goto release;
    }

    raw_data = bh->b_data;
    if (blk_nr / 8 >= bh->b_size) {
        printk(SFS_KERN_LEVEL "Too large blk_nr to test:[%llu]. aborted.\n",
               blk_nr);
        goto release;
    }
    raw_data += blk_nr / 8;
    *raw_data |= 1 << (blk_nr % 8);

    err = 0;
release:
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    return err;
}

/* update bitmap. 0 on success */
int sfs_update_ino_bmp_bit(struct super_block *sb, uint64_t ino_nr) {
    struct buffer_head *bh;
    char *raw_data;
    int err = -EINVAL;

    bh = sb_bread(sb, SFS_S_INFO(sb)->sfs_ino_bitmap);
    if (unlikely(!bh)) {
        SFSD(SFS_KERN_LEVEL "FAIL sb_bread()!!\n");
        goto release;
    }

    raw_data = bh->b_data;
    if (ino_nr / 8 >= bh->b_size) {
        printk(SFS_KERN_LEVEL "Too large ino_nr to test:[%llu]. aborted.\n", 
               ino_nr);
        goto release;
    }
    raw_data += ino_nr / 8;
    *raw_data |= 1 << (ino_nr % 8);

    err = 0;
release:
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    return err;
}

/* search for a entry. inode number on sucess, 0 on fail(0 is the root ino) */
unsigned long __sfs_search_dir_blk(struct super_block *sb, unsigned int blk_nr,
                                   const char *name) {
    struct buffer_head *bh;
    char *data;
    int i;
    unsigned int ino = 0;

    bh = sb_bread(sb, SFS_S_INFO(sb)->sfs_blk_start + blk_nr);
    BUG_ON(!bh);
    if (!bh) {
        SFSD(SFS_KERN_LEVEL "FAIL sb_bread()\n");
        goto final;
    }

    data = bh->b_data;

    if (!SFS_BLK_SIZE / SFS_FNAME_MAX) {
        SFSD(SFS_KERN_LEVEL "modulus error\n");
        goto final;
    }
    /* sfs have max 14 bytes name */
    for (i = 0; i < SFS_BLK_SIZE / SFS_FNAME_MAX; i++)
        if (0 == strncmp(&data[i * SFS_FNAME_MAX], name, SFS_FNAME_MAX))
            break;
    if (i == SFS_BLK_SIZE / SFS_FNAME_MAX) {
        SFSD(SFS_KERN_LEVEL "cannot find name in this block\n");
        goto final;
    }
    /* we use only two byte(uint16_t) to store ino_nr */
    ino = (unsigned int)(*((uint16_t *)(&data[i * SFS_FNAME_MAX + 14])));

final:
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    return (unsigned long)ino;
}

/* 
 * search to see whether child is present under a parent dir.
 * If present, retunr its ino, else return 0, which is the ino
 * of root dir, indicating absence
 */
unsigned long sfs_search_for_ino(struct super_block *sb,
                                 struct sfs_inode_info *parent_sii, const char *name) {
    unsigned int directs[SFS_INO_NDIRECT];
    unsigned int indirect;
    int i, ino;

    for (i = 0; i < SFS_INO_NDIRECT; i++)
        directs[i] = parent_sii->directs[i];
    indirect = parent_sii->indirect;

    for (i = 0; i < SFS_INO_NDIRECT; i++) {
        if (likely(directs[i] > 0)) {
            ino = __sfs_search_dir_blk(sb, directs[i], name);
            if (ino > 0) {
                return ino;
            }
        } else {
            break;
        }
    }
    printk(SFS_KERN_LEVEL "sfs_search_for_ino() cannot find child\n");
    return 0;
}

int __sfs_copy_single_blk_data(struct super_block *sb, char *dest, unsigned int blk_nr) {
    struct buffer_head *bh;

    bh = sb_bread(sb, SFS_S_INFO(sb)->sfs_blk_start + blk_nr);
    if (!bh) {
        SFSD(SFS_KERN_LEVEL "FAIL sb_bread()! \n");
        return -ENOMEM;
    }

    memcpy(dest, bh->b_data, SFS_BLK_SIZE);

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    return 0;
}

/* ============= end helper function ==================*/

/*
 * TheVFS calls this function from the creat() and open() system calls
 * to create a new inode associated with the given dentry object with the
 * specified initial access mode
 */
static int sfs_create(struct inode *dir, struct dentry *dentry,
                      umode_t mode, bool excl);

/*
 * This function searches a directory for an inode corresponding to a 
 * filename specified in the given dentry
 */
static struct dentry *sfs_lookup(struct inode *parent_inode,
                                 struct dentry *child_dentry, unsigned int flags);

/*
 * Called from the mkdir() system call to create a new directory with
 * the given initial mode
 */
static int sfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);

/*
 * Called from the unlink() system call to remove the inode specified by the
 * directory entry dentry from the directory dir
 */
static int sfs_unlink(struct inode *dir, struct dentry *dentry);

/*
 * Called by the rmdir() system call to remove the directory referenced by
 * dentry from the directory dir
 */
static int sfs_rmdir(struct inode *dir, struct dentry *dentry);

/* 
 * called when the VFS needs to read the directory contents
 */
static int sfs_iterate(struct file *filp, struct dir_context *ctx);

/* 
 * Reads len bytes from the given file at position offset into buf.
 * The file pointer is then updated.This function is called by the read() 
 * system call
 */
ssize_t sfs_read(struct file *filp, char __user *buf, size_t len,
                 loff_t *ppos);

/* 
 * Writes count bytes from buf into the given file at position offset.
 * The file pointer is then updated.This function is called by the write()
 * system call
 */
ssize_t sfs_write(struct file *filp, const char __user *buf, size_t len,
                  loff_t *ppos);

static struct file_operations sfs_file_ops = {
    .read = sfs_read,
    .write = sfs_write,
};

static struct file_operations sfs_dir_ops = {
    .owner = THIS_MODULE,
    .iterate = sfs_iterate,
};

static struct inode_operations sfs_inode_ops = {
    .create = sfs_create,
    .lookup = sfs_lookup,
    .mkdir = sfs_mkdir,
    .unlink = sfs_unlink,
    .rmdir = sfs_rmdir,
};

/*
 * the last argument, excl, means that this file have to be created
 * "exclusively", i.e., it can't exist before creating. we ignore this flag
 * for simplicity 
 */
static int sfs_create(struct inode *dir, struct dentry *dentry,
                      umode_t mode, bool excl) {
    struct super_block *sb;
    struct inode *inode;
    struct sfs_inode_info *sii, *parent_sii, *tmp_sii;
    struct buffer_head *bh, *bh2;
    uint32_t directs[SFS_INO_NDIRECT];
    uint32_t indirect;
    int ino_nr, err, i, j, k;
    char *raw_data;
    const char *filename;

    filename = dentry->d_name.name;
    if (unlikely(strlen(filename) > SFS_FNAME_MAX - 2)) {
        SFSD(SFS_KERN_LEVEL "length of filename exceed SFS_FNAME_MAX!\n");
        return -EINVAL;
    }

    sb = dir->i_sb;

    ino_nr = __sfs_get_next_inode_nr(sb);
    if (ino_nr < 0) {
        printk(SFS_KERN_LEVEL "inode bitmap full !!!\n");
        return -ENOMEM;
    }
    printk(SFS_KERN_LEVEL "next inode nr: [%d]\n", ino_nr);

    if (!S_ISDIR(mode) && !S_ISREG(mode)) {
        SFSD(SFS_KERN_LEVEL "creation request neither a file or directory."
                             "NOT SUPPORTED YET.\n");
        return -EINVAL;
    }

    inode = new_inode(sb);
    if (!inode) {
        SFSD(SFS_KERN_LEVEL "FAIL new_inode() !!\n");
        return -ENOMEM;
    }
    inode->i_sb = sb;
    inode->i_op = &sfs_inode_ops;
    inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
    inode->i_ino = ino_nr;
    inode->i_mode |= mode;
    atomic_set(&inode->i_count, 1); /* i_count: reference counter */
    set_nlink(inode, 1);            /* i_nlink: number of hard links */

    sii = kmem_cache_alloc(sfs_inode_cachep, GFP_KERNEL);
    if (unlikely(!sii)) {
        SFSD(SFS_KERN_LEVEL "FAIL kmem_cache_alloc() !\n");
        return -ENOMEM;
    }
    sii->inode_no = inode->i_ino;
    sii->slot_nr = ino_nr;
    inode->i_private = sii;
    sii->mode = mode;

    if (S_ISDIR(mode)) {
        printk(SFS_KERN_LEVEL "New directory creation request. name:[%s]\n",
               filename);
        inode->i_size = (loff_t)SFS_BLK_SIZE;
        sii->file_size = (unsigned long)SFS_BLK_SIZE;
        sii->directs[0] = __sfs_get_unused_blk(sb);
        if (!sii->directs[0]) { /* we are running out of block */
            SFSD(SFS_KERN_LEVEL "FAIL __sfs_get_unused_blk() \n");
            sii->directs[0] = 0;
        }
        err = sfs_update_blk_bmp_bit(sb, sii->directs[0]);
        if (err) {
            SFSD(SFS_KERN_LEVEL "FAIL sfs_update_blk_bmp_bit()!|n");
            return -ENOMEM;
        }
        inode->i_fop = &sfs_dir_ops;
    } else if (S_ISREG(mode)) {
        printk(SFS_KERN_LEVEL "New file creation request name:[%s]\n",
               filename);
        sii->file_size = 0;
        inode->i_size = 0;
        inode->i_fop = &sfs_file_ops;
    } else {
        printk(SFS_KERN_LEVEL "DONT know this new file creation request\n");
        return -EINVAL;
    }

    /* update child data */
    err = sfs_update_ino_bmp_bit(sb, ino_nr);
    if (!(0 == err)) {
        SFSD(SFS_KERN_LEVEL "FAIL sfs_update_ino_bmp_bit(). abort\n");
        return err;
    }
    err = sfs_update_prealloc_inodes(sb, sii);
    if (!(0 == err)) {
        SFSD(SFS_KERN_LEVEL "FAIL sfs_update_prealloc_inodes(). abort\n");
        return err;
    }

    /* update parent dir meta-data(make a new entry) */
    parent_sii = SFS_I_INFO(dir);
    bh = sb_bread(sb, SFS_S_INFO(sb)->sfs_ino_start);
    /* search for the sii. I know this would be slow(maybe use ino_nr later*/
    tmp_sii = (struct sfs_inode_info *)bh->b_data;
    for (i = 0; i < bh->b_size / sizeof(struct sfs_inode_info); i++)
        if (parent_sii->inode_no == (tmp_sii + i)->inode_no)
            break;
    if (i == bh->b_size / sizeof(struct sfs_inode_info)) {
        printk(SFS_KERN_LEVEL "search through all slot in preallocated inodes blk"
                          "but cannot find the same dir inode.\n");
        err = -EINVAL;
        goto release_bh;
    }
    tmp_sii = tmp_sii + i;

    /* to get where the dir data is placed */
    memcpy(directs, tmp_sii->directs, sizeof(uint32_t) * SFS_INO_NDIRECT);
    indirect = tmp_sii->indirect;

    for (i = 0; i < SFS_INO_NDIRECT; i++)
        if (0 == directs[i])
            break;
    if (i == SFS_INO_NDIRECT) {
        printk(SFS_KERN_LEVEL "WARNING: dir meta-file too large(use all"
                          "direct blk)");
        /*NOTE we donot use indirect blk here */
    }

    i = i == 0 ? 1 : i;
    printk(SFS_KERN_LEVEL "going to update dir entry. i:[%d], directs[i-1]:[%d]\n",
           i, directs[i - 1]);
    for (k = i - 1; k <= i && k != SFS_INO_NDIRECT; k++) {
        if (directs[k] == 0) {
            directs[k] = __sfs_get_unused_blk(sb);
            if (directs[k] == 0) {
                SFSD(SFS_KERN_LEVEL "FAIL __sfs_get_unused_blk() !\n");
                goto release_bh;
            }
            printk(SFS_KERN_LEVEL "__sfs_get_unused_blk:[%d]\n", directs[k]);
            /* update new blk info */
            parent_sii->directs[k] = tmp_sii->directs[k] = directs[k];
            sfs_update_blk_bmp_bit(sb, directs[k]);
        }
        /* to see whether the last filled block have some space left */
        bh2 = sb_bread(sb, SFS_S_INFO(sb)->sfs_blk_start + directs[k]);
        if (unlikely(!bh2)) {
            SFSD(SFS_KERN_LEVEL "FAIL sb_read() when reading from sfs_blk_start\n");
            err = -ENOMEM;
            goto release_bh;
        }
        raw_data = bh2->b_data;
        for (j = 0; j < SFS_BLK_SIZE / SFS_FNAME_MAX; j++)
            if (raw_data[j * SFS_FNAME_MAX] == '\0')
                break;
        if (j != SFS_BLK_SIZE / SFS_FNAME_MAX) {
            /* then there is still some space left in this blk */
            sprintf(raw_data + j * SFS_FNAME_MAX, "%s", filename);
            *((uint16_t *)(raw_data + j * SFS_FNAME_MAX + 14)) = (uint16_t)ino_nr; /* wooo~~ */
            printk(SFS_KERN_LEVEL "successfully update dir. added child entry\n");
            break;
        } else {
            /* then no space left for a new entry, so we use the next blk */
            if (k == i - 1) { /* first time reach here */
                mark_buffer_dirty(bh2);
                sync_dirty_buffer(bh2);
                brelse(bh2);
            }
        }
    }

    inode_init_owner(inode, dir, mode);
    d_add(dentry, inode);

    err = 0;
    SFSD(SFS_KERN_LEVEL "Going to sync/release bh2 and bh now\n");

    mark_buffer_dirty(bh2);
    sync_dirty_buffer(bh2);
    brelse(bh2);

release_bh:
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    return err;
}

/* 
 * @parent_inode: parent dir inode to search
 * @child_dentry: a negative dentry which we want to point to the found inode
 * (if inode found, we return this child_dentry after connecting it and the 
 * found inode, otherwise NULL is return )
 */
static struct dentry *sfs_lookup(struct inode *parent_inode,
                                 struct dentry *child_dentry, unsigned int flags) {
    struct buffer_head *bh;
    struct super_block *sb;
    struct sfs_inode_info *parent_sii, *sii, *tmp_sii;
    struct inode *inode;
    unsigned long ino;

    if (!S_ISDIR(parent_inode->i_mode)) {
        SFSD(SFS_KERN_LEVEL "performing sfs_lookup() on a non-dir !\n");
        return NULL;
    }

    parent_sii = SFS_I_INFO(parent_inode);
    if (unlikely(!parent_sii)) {
        SFSD(SFS_KERN_LEVEL "No i_private field in a inode! Fatal err\n");
        return NULL;
    }
    sb = parent_inode->i_sb;

    ino = sfs_search_for_ino(sb, parent_sii, child_dentry->d_name.name);
    if (ino == 0) { /* it can't be 0, which is root ino */
        printk(SFS_KERN_LEVEL "FAIL: sfs_lookup() fail to find required child under dir\n");
        return NULL;
    }

    sii = (struct sfs_inode_info *)kzalloc(sizeof(struct sfs_inode_info),
                                           GFP_KERNEL);
    if (!sii) {
        SFSD(SFS_KERN_LEVEL "FAIL kzalloc() !\n");
        return NULL;
    }
    bh = sb_bread(sb, SFS_S_INFO(sb)->sfs_ino_start);
    if (!bh) {
        SFSD(SFS_KERN_LEVEL "FAIL sb_read() 4 !\n");
        return NULL;
    }
    tmp_sii = (struct sfs_inode_info *)bh->b_data;
    tmp_sii += parent_inode->i_ino;
    memcpy(sii, tmp_sii, sizeof(struct sfs_inode_info));

    /*
     * every time we perform a loopup, and found that sfs_inode_info
     * we create a new in-memory inode and sfs_inode_info for it
     */
    inode = new_inode(sb);
    inode->i_ino = ino;
    /* leave the i_count ... */
    inode_init_owner(inode, parent_inode, sii->mode);
    inode->i_sb = sb;
    inode->i_op = &sfs_inode_ops;
    if (S_ISDIR(inode->i_mode))
        inode->i_fop = &sfs_dir_ops;
    else if (S_ISREG(inode->i_mode))
        inode->i_fop = &sfs_file_ops;
    else {
        SFSD(SFS_KERN_LEVEL "unknown inode type!(neither dir or regular "
                             "file. i_mode: [0x%x]\n",
                  (unsigned)inode->i_mode);
        mark_buffer_dirty(bh);
        sync_dirty_buffer(bh);
        brelse(bh);
        return NULL;
    }
    /* FIXME: we should store these time on disk and retrive them */
    inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
    inode->i_mode |= sii->mode;
    inode->i_size = sii->file_size;
    inode->i_private = sii;

    /*
     * FIXME: (from doc)If the named inode does not exist a NULL inode
	 * should be inserted into the dentry (this is called a negative
	 * dentry). Returning an error code from this routine must only
	 * be done on a real error, otherwise creating inodes with system
	 * calls like create(2), mknod(2), mkdir(2) and so on will fail.
     */
    d_add(child_dentry, inode);

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    return child_dentry;
}

static int sfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode) {
    return sfs_create(dir, dentry, mode | S_IFDIR, 1);
}

/* helper function */
static int sfs_remove(struct inode *dir, struct dentry *dentry) {
    struct super_block *sb;
    struct buffer_head *bh;
    struct inode *inode;
    struct sfs_inode_info *sii, *parent_sii;
    char *data;
    int i, j, set = 0;

    inode = dentry->d_inode;
    sii = SFS_I_INFO(inode);
    parent_sii = SFS_I_INFO(dir);
    sb = inode->i_sb;

    /* clear all contents of this file */
    for (i = 0; i < SFS_INO_NDIRECT; i++) {
        bh = sb_bread(sb, SFS_S_INFO(sb)->sfs_blk_start + sii->directs[i]);
        if (!bh) {
            SFSD(SFS_KERN_LEVEL "FAIL sb_bread()\n");
            return -ENOMEM;
        }
        data = bh->b_data;
        memset(data, '\0', sb->s_blocksize);
        sii->directs[i] = 0;
        mark_buffer_dirty(bh);
        sync_dirty_buffer(bh);
        brelse(bh);
    }
    /* clear inode */
    bh = sb_bread(sb, SFS_S_INFO(sb)->sfs_ino_start);
    if (!bh) {
        SFSD(SFS_KERN_LEVEL "FAIL sb_bread()\n");
        return -ENOMEM;
    }
    data = (char *)((struct sfs_inode_info *)bh->b_data + sii->inode_no);
    memset(data, '\0', sizeof(struct sfs_inode_info));
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    /* free sii memory */
    kmem_cache_free(sfs_inode_cachep, sii);

    /* free dir entry */
    for (i = 0; i < SFS_INO_NDIRECT; i++) {
        if (set)
            break;
        bh = sb_bread(sb,
                      SFS_S_INFO(sb)->sfs_blk_start + parent_sii->directs[i]);
        if (!bh) {
            SFSD(SFS_KERN_LEVEL "FAIL sb_bread()\n");
            return -ENOMEM;
        }
        data = bh->b_data;
        for (j = 0; j < SFS_BLK_SIZE / SFS_FNAME_MAX; j++) {
            /* FIXME: when fill in dir entry, we only use the last blk. but if
             * we leave a hole here if would not get used */
            data += j * SFS_FNAME_MAX;
            if (0 == strncmp(data, dentry->d_name.name, SFS_FNAME_MAX - 2)) {
                memset(data, '\0', SFS_FNAME_MAX);
                set = 1;
                break;
            }
        }
        mark_buffer_dirty(bh);
        sync_dirty_buffer(bh);
        brelse(bh);
    }

    inode_dec_link_count(inode);

    return 0;
}

static int sfs_unlink(struct inode *dir, struct dentry *dentry) {

    if (!S_ISREG(dentry->d_inode->i_mode)) {
        SFSD(SFS_KERN_LEVEL "FAIL unlink(): not a regular file!\n");
        return -EINVAL;
    }

    return sfs_remove(dir, dentry);
}

/*
 * FIXME: we are not obeying the rule !!! Calling sfs_rmdir() would delete this
 * directory whether or not it is empty
 */
static int sfs_rmdir(struct inode *dir, struct dentry *dentry) {

    if (!S_ISDIR(dentry->d_inode->i_mode)) {
        SFSD(SFS_KERN_LEVEL "FAIL unlink(): not a regular file!\n");
        return -EINVAL;
    }

    return sfs_remove(dir, dentry);
}

/*
 * called when the VFS needs to read the directory contents
 */
static int sfs_iterate(struct file *filp, struct dir_context *ctx) {
    loff_t pos;
    struct buffer_head *bh;
    struct super_block *sb;
    struct inode *inode;
    struct sfs_inode_info *sii;
    int i, j;
    char *raw_data;

    pos = ctx->pos;

    /* FIXME: if you don't return here, then sfs_iterate() would be called
     * infinitely when you Tab-auto-complete a filename */
    if (pos)
        return 0;

    /*
     * up to kernel version 3.18, there still filp->f_dentry, which is defined
     * as: `#define f_dentry f_path.f_dentry'. But in 3.19, it was gone
     */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
    inode = filp->f_path.dentry->d_inode;
#else
    inode = filp->f_dentry->d_inode;
#endif

    sb = inode->i_sb;
    sii = SFS_I_INFO(inode);
    if (unlikely(!S_ISDIR(sii->mode))) {
        printk(SFS_KERN_LEVEL
               "[%s] with inode [%lu](sii->inode_no[%lu]) is NOT a directory !\n",
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
               filp->f_path.dentry->d_name.name,
#else
               filp->f_dentry->d_name.name,
#endif
               inode->i_ino, sii->inode_no);
        return -ENOTDIR;
    }

    SFSD(SFS_KERN_LEVEL "DEBUGGING: ctx->pos = %lu \n", (unsigned long)pos);

    /* NOTE: we are NOT using the indirect block here */
    for (i = 0; sii->directs[i] != 0 && i < SFS_INO_NDIRECT; i++) {
        char filename[SFS_FNAME_MAX + 1];
        uint16_t ino;
        bh = sb_bread(sb, SFS_S_INFO(sb)->sfs_blk_start + sii->directs[i]);
        if (!bh) {
            SFSD(SFS_KERN_LEVEL "FAIL sb_bread() !\n");
            return -ENOMEM;
        }
        raw_data = bh->b_data;
        for (j = 0; j < SFS_BLK_SIZE / SFS_FNAME_MAX; j++) {
            memset(filename, '\0', SFS_FNAME_MAX + 1);
            if (raw_data[j * SFS_FNAME_MAX] == '\0')
                goto release_and_break;
            strncpy(filename, &raw_data[j * SFS_FNAME_MAX], SFS_FNAME_MAX);
            printk(SFS_KERN_LEVEL "filename copied:[%s]\n", filename);
            ino = *((uint16_t *)((&raw_data[j * SFS_FNAME_MAX]) + 14));
            dir_emit(ctx, filename, SFS_FNAME_MAX, (uint32_t)ino, DT_UNKNOWN);
            /* TODO: need clarification */
            ctx->pos += SFS_FNAME_MAX;
        }
        brelse(bh);
        continue;
    release_and_break:
        brelse(bh);
        break;
    }
    return 0;
}

/*
 * NOTE: when reading a large file, the kernel would through out such warning
 * and abort the reading:
 * atkbd serio0: Spurious NAK on isa0060/serio0. Some program might be trying to
 * acess hardware directly
 * (but with small file it's really ok)
 */
ssize_t sfs_read(struct file *filp, char __user *buf, size_t len,
                 loff_t *ppos) {
    struct buffer_head *bh;
    struct super_block *sb;
    struct inode *inode;
    struct sfs_inode_info *sii;
    char *kbuf, *raw_data;
    int nbytes, slot, blk_nr, frag_size, ret, i, k;

    /*
     * in newer version kernel, we can use filp->f_inode instead of
     * f->f_path.dentry->d_inode 
     */
    inode = filp->f_path.dentry->d_inode;
    sii = SFS_I_INFO(inode);
    if (*ppos >= sii->file_size) {
        return 0;
    }
    sb = inode->i_sb;
    SFSD(SFS_KERN_LEVEL "file postion:[%ld], file size:[%ld]\n",
              (long int)*ppos, sii->file_size);

    /*
     * The if clause can be omitted, since we have already check sii->file_size.
     * However we want to keep this for debugging in case file_size and
     * sii->indirects is not in agree
     */
    slot = *ppos / SFS_BLK_SIZE;
    if (slot > SFS_INO_NDIRECT) {
        /* we only use direct slot now */
        SFSD(SFS_KERN_LEVEL "sfs_read() attempting to read more than 10 direct block !\n");
        return 0;
    }
    blk_nr = sii->directs[slot];
    /* we can read at most this frag from this blk */
    frag_size = SFS_BLK_SIZE - (*ppos % SFS_BLK_SIZE);
    /* we can read at most this length from the file */
    nbytes = min((size_t)(sii->file_size - *ppos), len);
    /* assume that we have enough memory(or see sfs_write() ) */
    kbuf = (char *)kzalloc(nbytes + SFS_BLK_SIZE, GFP_KERNEL);
    if (!kbuf) {
        SFSD(SFS_KERN_LEVEL "FAIL kzalloc() \n");
        return 0;
    }

    bh = sb_bread(sb, SFS_S_INFO(sb)->sfs_blk_start + blk_nr);
    if (!bh) {
        SFSD(SFS_KERN_LEVEL "FAIL sb_read() 5 !\n");
        return 0;
    }
    raw_data = bh->b_data + SFS_BLK_SIZE - frag_size;
    if (nbytes < frag_size) { /* we can perform all read in this single blk */
        /*
         * copy_to_user() return 0 on success. return the number of bytes they
         * fail to copy on error
         */
        k = copy_to_user(buf, raw_data, nbytes);
        if (0 != k) {
            SFSD(SFS_KERN_LEVEL
                      "FAIL copy_to_user()! fail to copy [%d] bytes\n",
                      k);
            ret = 0;
            goto release;
        }
    } else { /* we need to read more blk to satisfy this read request */
        memcpy(kbuf, raw_data, frag_size);
        kbuf += frag_size;
        nbytes -= frag_size;
        slot++;
        for (i = 0; i <= nbytes / SFS_BLK_SIZE; i++) {
            k = __sfs_copy_single_blk_data(sb, kbuf, sii->directs[slot + i]);
            if (0 != k) {
                SFSD(SFS_KERN_LEVEL "FAIL __sfs_copy_single_blk_data()!\n");
                ret = 0;
                goto release;
            }
            kbuf += SFS_BLK_SIZE;
        }
        /* copy the data left */
        if (nbytes % SFS_BLK_SIZE) {
            k = __sfs_copy_single_blk_data(sb, kbuf, sii->directs[slot + i]);
            if (0 != k) {
                SFSD(SFS_KERN_LEVEL "FAIL __sfs_copy_single_blk_data()!\n");
                ret = 0;
                goto release;
            }
        }

        nbytes = nbytes + frag_size;
        k = copy_to_user(buf, kbuf, nbytes);
        if (0 != k) {
            SFSD(SFS_KERN_LEVEL "FAIL copy_to_user()! bytes copied:[%d]\n", k);
            ret = 0;
            goto release;
        }
    }
    *ppos += nbytes;
    ret = nbytes;

release:
    kfree(kbuf);
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    return ret;
}

ssize_t sfs_write(struct file *filp, const char __user *buf, size_t len,
                  loff_t *ppos) {
    struct super_block *sb;
    struct buffer_head *bh;
    struct inode *inode;
    struct sfs_inode_info *sii;
    size_t slot, frag_size, chunk;
    char *data;
    int i, j, k, ret;

    ret = generic_write_checks(filp, ppos, &len, 0);
    if (ret) {
        SFSD(SFS_KERN_LEVEL "FAIL generic_write_checks() !\n");
        return 0;
    }

    /*
     * in newer version kernel, we can use filp->f_inode instead of
     * f->f_path.dentry->d_inode 
     */
    inode = filp->f_path.dentry->d_inode;
    sii = SFS_I_INFO(inode);
    sb = inode->i_sb;

    if (*ppos + len > SFS_INO_NDIRECT * SFS_BLK_SIZE) {
        SFSD(SFS_KERN_LEVEL "maximum file size exceed when writing!\n");
        return 0;
    }
    slot = *ppos / SFS_BLK_SIZE;
    if (sii->directs[slot] == 0) {
        /* acquire new block */
        sii->directs[slot] = __sfs_get_unused_blk(sb);
        if (0 == sii->directs[slot]) {
            SFSD(SFS_KERN_LEVEL "FAIL __sfs_get_unused_blk()!\n");
            return 0;
        }
        ret = sfs_update_blk_bmp_bit(sb, sii->directs[slot]);
        if (ret) {
            SFSD(SFS_KERN_LEVEL "FAIL sfs_update_blk_bmp_bit()!|n");
            return 0;
        }

        /* also acquire new block for < slot (i.e., this file contain hole) */
        for (i = 0; i < slot; i++) {
            sii->directs[i] = __sfs_get_unused_blk(sb);
            if (0 == sii->directs[i]) {
                /* FIXME: this situation is awkward, we have to roll back */
                SFSD(SFS_KERN_LEVEL "FAIL __sfs_get_unused_blk()"
                                     "for < slot !...awkward...\n");
                return 0;
            }
            ret = sfs_update_blk_bmp_bit(sb, sii->directs[slot]);
            if (ret) {
                SFSD(SFS_KERN_LEVEL "FAIL sfs_update_blk_bmp_bit()!|n");
                return 0;
            }
        }
    }
    frag_size = SFS_BLK_SIZE - (*ppos % SFS_BLK_SIZE);
    bh = sb_bread(sb, SFS_S_INFO(sb)->sfs_blk_start + sii->directs[slot]);
    if (!bh) {
        SFSD(SFS_KERN_LEVEL "FAIL sb_bread() !\n");
        return 0;
    }
    data = bh->b_data;
    chunk = min(len, frag_size);
    /* we should probably check this return value to depress compiler warning
     * (unused return value) */
    copy_from_user(data + (*ppos % SFS_BLK_SIZE), buf, chunk);
    buf += chunk;
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    /* file size changed, record it into sii and inode */
    sii->file_size += chunk;
    if (0 != sfs_update_prealloc_inodes(sb, sii)) {
        SFSD(SFS_KERN_LEVEL "FAIL sfs_update_prealloc_inodes() !\n");
        return 0;
    }
    inode->i_size += chunk;
    if (len < frag_size) {
        *ppos += len;
        return len;
    } else {
        /* 
         * need more blks to store these data.
         * we don't need chech file size overflow here because we have
         * already done it above
         */
        chunk = len - chunk; /* how many bytes left to transfer */
        k = chunk / SFS_BLK_SIZE;
        for (j = 0; j < k + 1; j++) {
            slot++;
            if (sii->directs[slot] == 0) {
                sii->directs[slot] = __sfs_get_unused_blk(sb);
                if (0 == sii->directs[slot]) {
                    SFSD(SFS_KERN_LEVEL "FAIL __sfs_get_unused_blk()!\n");
                    return 0;
                }
                ret = sfs_update_prealloc_inodes(sb, sii);
                if (ret) {
                    SFSD(SFS_KERN_LEVEL "FAIL sfs_update_prealloc_inodes()!|n");
                    return 0;
                }
                ret = sfs_update_blk_bmp_bit(sb, sii->directs[slot]);
                if (ret) {
                    SFSD(SFS_KERN_LEVEL "FAIL sfs_update_blk_bmp_bit()!|n");
                    return 0;
                }
            }
            bh = sb_bread(sb, SFS_S_INFO(sb)->sfs_blk_start + sii->directs[slot]);
            if (!bh) {
                SFSD(SFS_KERN_LEVEL "FAIL sb_bread()!\n");
                return 0;
            }
            /* we should propably check this return value to depress compiler
             * warning(unused return value) */
            copy_from_user(bh->b_data, buf, min(chunk, (size_t)SFS_BLK_SIZE));
            sii->file_size += min(chunk, (size_t)SFS_BLK_SIZE);
            /* file size changed. recode that into sii and inode */
            if (0 != sfs_update_prealloc_inodes(sb, sii)) {
                SFSD(SFS_KERN_LEVEL "FAIL sfs_update_prealloc_inodes() !\n");
                return 0;
            }
            inode->i_size += min(chunk, (size_t)SFS_BLK_SIZE);
            chunk -= min(chunk, (size_t)SFS_BLK_SIZE);
            mark_buffer_dirty(bh);
            sync_dirty_buffer(bh);
            brelse(bh);
        }
        *ppos += len;
        return len;
    }
}

/* Usually, this is not needed if alloc_inode() is not defined */
/*
static void sfs_destroy_inode(struct inode *inode) {
    struct sfs_inode_info *sii = SFS_I_INFO(inode);
    kmem_cache_free(sfs_inode_cachep, sii);
    SFSD(SFS_KERN_LEVEL "freeing sfs_inode_info...\n");
}
*/

/*
static const struct super_operations sfs_sb_ops = {
    .alloc_inode   = sfs_alloc_inode,
    .destroy_inode = sfs_destroy_inode,
    .put_super     = sfs_put_super,
    .write_inode   = sfs_write_inode,
    .evict_inode   = sfs_evict_inode,
    .sync_fs       = sfs_sync_fs,
    .statfs        = sfs_statfs,
    .remount_fs    = sfs_remount,
    .show_options  = ufs_show_options,
    .dirty_inode   = NULL,
    .drop_inode    = NULL,
    .freeze_super  = NULL,
    .freeze_fs     = NULL,
    .thaw_super    = NULL,
    .unfreeze_fs   = NULL,
    .umount_begin  = NULL,
    .show_devname  = NULL,
    .show_path     = NULL,
    .show_stats    = NULL,
    .bdev_try_to_free_page = NULL,
    .nr_cached_objects     = NULL,
    .free_cached_objects   = NULL,
};
*/

/* 
 * when mounting sfs, VFS call `sfs_mount', which in turn call `mount_bdev',
 * which in turn call `sfs_fill_sb'. In these procedures, 
 * - `mount_bdev' would establishe the connection between filesystem and the
 *     corresponding device(think vfsmount)
 * - `sys_fill_sb' is the one who do the real initialization job for sb...
 */
/*
 * @sb: the superblock structure. should be initialize properly
 * @data: arbitrary mount options, usually comes as an ASCII string(see the doc)
 * @silent: whether or not to be silent on error
 */
static int sfs_fill_sb(struct super_block *sb, void *data, int silent) {
    struct sfs_sb_info *sbi;
    struct inode *ri;
    struct buffer_head *bh;

    sbi = kzalloc(sizeof(struct sfs_sb_info), GFP_KERNEL);
    if (unlikely(!sbi)) {
        SFSD(SFS_KERN_LEVEL "FAIL alloca memory for sbi !!!");
        return -EINVAL;
    }

    printk(SFS_KERN_LEVEL "The original sb blksize is:[%lu]", sb->s_blocksize);
    bh = sb_bread(sb, SFS_SB_START_NR);
    BUG_ON(!bh);

    /* bh would eventually be freed, so we use other place to place info */
    memcpy(sbi, (struct sfs_sb_info *)bh->b_data, sizeof(struct sfs_sb_info));
    printk(SFS_KERN_LEVEL
           "Obtained from disk: magic[0x%lx],version[0x%lx] blk_size[%lu]\n",
           sbi->magic, sbi->version, sbi->blk_size);
    if (unlikely(sbi->magic != SFS_MAGIC_NUMBER)) {
        printk(SFS_KERN_LEVEL "FAIL check magic number !!!"
                          "magic read:[0x%lx]\n",
               sbi->magic);
        kfree(sbi);
        brelse(bh);
        return -EINVAL;
    }
    if (unlikely(sbi->blk_size != SFS_BLK_SIZE)) {
        printk(SFS_KERN_LEVEL "FAIL check blk_size !!\n");
        kfree(sbi);
        brelse(bh);
        return -EINVAL;
    }

    printk(SFS_KERN_LEVEL "sfs of version[%lu] with blk size [%lu] detected.\n",
           sbi->version, sbi->blk_size);

    sb->s_magic = SFS_MAGIC_NUMBER;
    sb->s_fs_info = sbi;
    sbi->blk_size = sb->s_blocksize; /* s_blocksize is used by sb_bread() */

    /* maximum file size of this file system. would change in the future */
    sb->s_maxbytes = SFS_BLK_SIZE * 11;
    /*sb->s_op = &sfs_sb_ops;*/

    /* should we check mount options ? */

    ri = new_inode(sb);
    ri->i_ino = SFS_ROOTINO;
    atomic_set(&ri->i_count, 1);
    set_nlink(ri, 1);
    inode_init_owner(ri, NULL, S_IFDIR);
    ri->i_sb = sb;
    ri->i_op = &sfs_inode_ops;
    ri->i_fop = &sfs_dir_ops;
    ri->i_atime = ri->i_mtime = ri->i_ctime = CURRENT_TIME;
    ri->i_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IROTH;
    ri->i_size = (loff_t)SFS_BLK_SIZE;

    ri->i_private = sfs_get_inode(sb, SFS_ROOTINO);
    if (!ri->i_private) {
        SFSD(SFS_KERN_LEVEL "FAIL get root inode from disk. check you disk \n");
        kfree(sbi);
        brelse(bh);
        return -EINVAL;
    }

    /*
     * d_make_root() is responsible for establishing connection between
     * root entry(i.e., sb->s_root) and dentry of its parent 
     */
    sb->s_root = d_make_root(ri);
    if (!sb->s_root) {
        kfree(sbi);
        brelse(bh);
        return -EINVAL;
    }

    brelse(bh);
    return 0;
}

/*
 * @type: describe the filesystem, partly initialized by specific filesystem
 *        code
 * @flags: mount flags(provided by user when mounting the file system)
 * @dev_name: the device name we are mounting
 * @data: arbitary mount options, usually come as an ASCII string(see the doc)
 *
 * This mount() method must return the root dentry of the tree requested by
 * caller.  An active reference to its superblock must be grabbed and the
 * superblock must be locked.  On failure it should return ERR_PTR(error).
 * (interesting hack: this mount() method may choose to return a subtree of
 * existing filesystem)
 */
static struct dentry *sfs_mount(struct file_system_type *type, int flags,
                                const char *dev, void *data) {
    /* `mount_bdev' will call sfs_fill_sb inside */
    struct dentry *const entry = mount_bdev(type, flags, dev,
                                            data, sfs_fill_sb);
    if (IS_ERR(entry))
        printk(SFS_KERN_LEVEL "FAILED mounting sfs \n");
    else
        printk(SFS_KERN_LEVEL "SUCCESS mount sfs on: [%s]\n", dev);
    return entry;
}

static void sfs_kill_block_super(struct super_block *sb) {
    struct inode *ri;
    struct sfs_inode_info *root_sii;

    printk(SFS_KERN_LEVEL "sfs_kill_blcok_super() get called. \n");

    ri = sb->s_root->d_inode;
    root_sii = SFS_I_INFO(ri);
    kmem_cache_free(sfs_inode_cachep, root_sii);
    inode_dec_link_count(ri);
    kill_block_super(sb);
}

static struct file_system_type sfs_filesystem_type = {
    /* `owner' field is necessary in order to setup a counter
     * of links to the module */
    .owner = THIS_MODULE,
    .name = "sfs",
    .mount = sfs_mount,
    .kill_sb = sfs_kill_block_super,
    .fs_flags = FS_REQUIRES_DEV,
};
MODULE_ALIAS_FS("sfs"); /* identification stuff ? */

static void destroy_inodecache(void) {
    kmem_cache_destroy(sfs_inode_cachep);
}

static int __init sfs_init(void) {
    int ret;

    /* inode cache that used to hold our in-memory sfs-inode */
    sfs_inode_cachep = kmem_cache_create("sfs_inode_cache",
                                         sizeof(struct sfs_inode_info),
                                         0,
                                         (SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD),
                                         NULL);
    if (!sfs_inode_cachep) {
        return -ENOMEM;
    }
    ret = register_filesystem(&sfs_filesystem_type);
    if (likely(ret == 0)) {
        printk(SFS_KERN_LEVEL "sfs module loaded\n");
        goto out1;
    } else {
        printk(SFS_KERN_LEVEL "Failed to loaded sfs !!!");
        goto out;
    }
out:
    destroy_inodecache();
out1:
    return ret;
}

static void __exit sfs_exit(void) {
    unregister_filesystem(&sfs_filesystem_type);
    destroy_inodecache();
    printk(SFS_KERN_LEVEL "sfs module unloaded\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yubin Ruan");
module_init(sfs_init);
module_exit(sfs_exit);
