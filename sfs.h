/* 
 *  fs/sfs/sfs.h
 *  created by Yubin Ruan, 2016 <my email...>
 *
 *
 * This file is part of the sfs filesystem source code, which is targeted at
 * Linux kernel version 3.1x-4.6x. All of the source code are licensed under  
 * the Creative Commons Zero License, a public domain license. You can
 * redistribute it or modify in any way you want. It is distributed in the hope
 * that it will be useful and educational for learning and hacking the Linux
 * kernel, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SILVER_FILESYSTEM_H
#define SILVER_FILESYSTEM_H

/*
 * sfs try to act as the original old and simple unix filesystem(not the ufs in
 * current linux kernel). Layout of sfs looks like this: 
 *  +--------+----+----------+----------+-------------------+------------+
 *  |boot sec| sb |ino_bitmap|blk_bitmap|preallocated inodes|data blks...|
 *  +--------+---------------+----------+-------------------+------------+
 *
 *  Note that currently we use one block for superblock, inode bitmap, blk
 *  bitmap and preallocated inodes(which means that there is very few inodes.I
 *  know), so the filesystem is very limited...
 *  And also, for simplicity, we leave an entire block for boot sector
 */

#ifdef SFS_DEBUG
#define SFS_KERN_LEVEL KERN_ALERT
#else
#define SFS_KERN_LEVEL KERN_INFO
#endif

#define SFS_MAGIC_NUMBER 0x19451001
#define SFS_BLK_SIZE 4096    /* default sfs logical block size */
#define SFS_SB_START_NR 1       /* where sb begin. default after boot sector */
#define SFS_MAX_LINK 1000   /* maxinum number of links */
#define SFS_FNAME_MAX 14

#define SFS_ROOTINO 0
#define SFS_ROOT_SLOT_NR 0
#define MAX_INODE (SFS_BLK_SIZE * 8)
#define SFS_INO_NDIRECT 10
#define SFS_INODE_WITHIN_RANGE(ino) \
    ( ino >= 0 && ino <= MAX_INODE)

/* on-memory/disk structure of sfs super block */
struct sfs_sb_info {
    unsigned long magic;
    unsigned long version;
    unsigned long blk_size;
    unsigned long sfs_ino_bitmap;         /* address of inode bitmap */
    unsigned long sfs_blk_bitmap;         /* address of block bitmap */
    unsigned long sfs_ino_start;
    unsigned long sfs_blk_start;    /* now it should be sfs_ino_start+1 */

    /*
     * we don't include this now, since mkfs.sfs.c will use
     * this header and it is NOT compiled against the kernel
     * headers, i.e., it don't know that `struct super_block' is
     */
    /*struct super_block *sb;*/
};

/* on-memory/disk structure of sfs inode */
struct sfs_inode_info {
    /*
     * we don't include this now, since mkfs.sfs.c will use
     * this header and it is NOT compiled against the kernel
     * header, i.e., it dont know what `struct inode' is
     */
    /*struct inode vfs_inode;*/

    mode_t mode;   /* hopefully this type is the same in kernel and glibc */
    unsigned long inode_no;
    /*NOTE: at this time ino_nr is always equal slot_nr(redundency) */
    unsigned long slot_nr;   /* which slot in prealloc inodes blk */
    unsigned int directs[SFS_INO_NDIRECT];   /* now it indicate blk. NOT inode nr */
    unsigned int indirect;
    unsigned long file_size;
};


/*
 * Debug utils
 */
#define SFSD(f, a...) {                 \
    printk("SFS_DEBUG: (%s, %d): %s:",      \
             __FILE__, __LINE__, __func__);  \
    printk(f, ## a);                       \
}




/* #### a few NOTE ####
 * ====== set textwidth=80 =======
 * --------------------------------
 * four primary object types of Linux VFS are:
 *   * struct superblock
 *   * struct inode
 *   * struct dentry
 *   * struct file
 * there is an `operations' contained within these object
 *   * struct super_operations
 *   * struct inode_operations
 *   * struct dentry_operations
 *   * struct file_operations
 * Each filesystem is registered by a `filesystem_type' struct,
 * Each mount point is represent by a `vfsmount' struct
 *
 * Each process has two struct `fs_struct' and `file' associated with
 * it, describing filesystem and file, respectively
 *
 * `struct super_block' and `struct super_operations' defined in <linux/fs.h>
 * Some generic superblock operation is in <fs/super.c>
 *
 * ...inode stuff
 *
 * ...dentry stuff
 * `dentry' objects are all components in a path, including files. For example,
 * in path `/bin/vi', / bin vi are all dentry objects
 * `dentry' is defined in <linux/dcache.h>
 * NOTE that dentry object does not corresponding to any sort of on-disk
 * structure(so that there is no flag indicatinf whether it's modified or not.)
 * VFS create it on-the-fly from a string representation of a path name.
 *
 * the `file' object is the in-memory representation of an open file(created
 * when someone use `open()'). i.e, ir represent process's view of a file. There
 * can be multiple `file' object in existence for the same file. The `file'
 * object points back to the dentry, which in turn points back to the inode,
 * that actually represents the open file(the inode and dentry objects, of
 * course, are unique):
 *      
 *         +----------+
 *         |file_obj_1| ----+
 *         +----------+     |        +----------+      +----------+
 *                          +----->  |dentry_obj| ---> | inode_obj|
 *                          +----->  +----------+      +----------+
 *         +----------+     |
 *         |file_obj_2|-----+
 *         +----------+
 * NOTE that, similar to the dentry object, the file object does not actually
 * correspond to any on-disk structure.
 *
 * `files_struct', `fs_struct' and `namespace'
 * `struct files_struct' is defined in <linux/fdtable.h>. This struct represent
 * a per-process information about open files and file descriptors. There is a
 * pointer in the process descriptor what point to this struct.
 * `struct fs_struct' is defined in <linux/fs_struct.h>, which contains
 * filesystem info related to a process and is pointed at by the `fs` field in
 * the process descriptor.
 * `namespace' is related to a process's view of the fs hierarchy....
 * NOTE that the difference between `struct file' and `struct files_struct' is
 * many-to-one: 
 *                                     +--------+    +----------+    +---------+
 *                           +-------->|file_obj|--->|dentry_obj|--->|inode_obj|
 *                           |         +--------+    +----------+    +---------+
 * +------------+   +------------+     +--------+    +----------+    +---------+
 * |process_desc|-->|files_struct|---->|file_obj|--->|dentry_obj|--->|inode_obj|
 * +------------+   +------------+     +--------+    +----------+    +---------+
 *                             |       +--------+    +----------+    +---------+ 
 *                             +------>|file_obj|--->|dentry_obj|--->|inode_obj|
 *                                     +--------+    +----------+    +---------+
 *
 * When a block is stored in memory, it's actually stored in a `buffer'.
 * `buffer_head', the descriptor of a buffer, hold all the info that the kernel
 * need to know about a buffer. The struct is defined in <linux/buffer_head.h>.
 * NOTE that a single buffer is larger than a sector but smaller than a page.
 * `struct bio' allow for a more powerful representation......
 *
 * `struct request_queue' and `stuct request'
 *    defined in <linux/blkdev.h>
 *    block devices maintain request queues to store their pending I/O requests.
 *    high-level code, such as filesystems, add requests to the request queue
 *
 * `I/O scheduler'
 *    An I/O scheduler works by managing a block device's request queue.
 *    Generally, an I/o scheduler perform _merging_ and _sorting_ of requests
 *    in request queue.
 * -- The "Linus elevator" I/O scheduler is implemented in <block/elevator.c>
 * -- The "Linus elevator" I/O scheduler greatly improve global throughput, but
 *    may starve some request(especially read request). "Deadline I/O scheduler"
 *    alleviate this problem by using three queue: 
 *        * normal sorted queue(the same as that in Linus elevator)
 *        * a write FIFO queue(in which each request with a expired time)
 *        * a read FIFO queue(in which each request with a expired time)
 *    when the front request of either of the FIFO queue expire, it must be 
 *    serviced. Otherwise, the normal sorted queue is used to served requests.
 *    Of course Deadline I/O scheduler reduce throughput.
 *    Implementation can be found at <block/deadline-iosched.c>
 * -- The Anticipatory I/O scheduler are based on Deadline I/O scheduler, but is 
 *    implemented with a "anticipated heuristic". When an request is issued, it
 *    is handled as usual. But when a request is submitted(finished), the 
 *    scheduler does not immediately seek back to serve another request, but 
 *    rather wait for a predefined time to see if there is another future
 *    request that acts near the current disk area. If the 'anticipation' is
 *    right the throughput improvement is gained, otherwise some time is wasted.
 *    Implementation can be found at <block/as-iosched.c>
 * -- The Complete Fair Queue I/O scheduler(CFQ)
 *      Now the default I/O scheduler
 *      Every process has its request queue. round robin
 *      Provide fairness at process level
 *      Implementation can be found at <block/cfq-iosched.c>
 * -- The Noop I/O scheduler
 *      Only perform _merging_
 *      suitable for device which is truly 'random access', such as flash memory
 *      card, in which there is no overhead associated with seeking(so sorting
 *      is not needed)
 */



#endif /* SILVER_FILESYSTEM_H */
