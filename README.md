# A simple filesystem for Linux
This is a simple filesystem for I make for when learning the Linux Kernel. It implement the basic *NIX filesystem
semantics and use the _pretty old_ unix filesystem model(inodes, bitmap, blocks, etc...). It can work like a normal filesystem
do. You can use `mkfs.sfs`, which is part of the source code, to create a filesystem in a device(or a normal file and then
Linux's loop device mechanism) and then mount it under a mount point. After that, you can create. remove file/dir and walking
in the filesystem.


## A few Illustration of how it work
First word, I'm ashamed of the brain-damage-and-dont-know-why design of this filesystem. As a software developer, I feel sorry
and guilty about makin such a limited and useless thing. Sorry. But I have my consideration. First I have to make thing work
so that I can see more clear about the kernel VFS mechanism, after which I can make further progress. Now that I do, I think
this ugly work make its contribution to me.

This filesystem try to act as the original old and simple unix filesystem(not the ufs in current linux kernel).
Layout of sfs looks like this: 
```text
 +-----------+-------------+----------+----------+-------------------+------------+
 |boot sector| super block |ino_bitmap|blk_bitmap|preallocated inodes|data blks...|
 +-----------+-------------+----------+----------+-------------------+------------+
```
 Currently we use one block for superblock, inode bitmap, blk bitmap and preallocated inodes(which means that there are
 very few inodes).
   * The first block, boot sector, is left blank. 
   * _super block_ records some meta data about the filesystem.
   * _ino_bitmap_ records whether the inode is in used. 
   * _blk_bitmap_ records whether some block is in used. 
   * _preallocated inodes_ stores all the inode. We use only one block to store inode, so there are (blk_size / inode_size)
     inode in the filesystem. As a result, there are more bit in the _ino_bitmap_ than there are inode in the
     _preallocated inodes_ block.
 
 The code use its own representation of inode in disk:
 ```c
 struct sfs_inode_info {
    mode_t mode;               /* inode mode, the same as `st_mode` in glibc */
    unsigned long inode_no;
    /*NOTE: at this time ino_nr is always equal slot_nr(redundency) */
    unsigned long slot_nr; /* which slot in prealloc inodes blk */
    unsigned int directs[SFS_INO_NDIRECT];
    unsigned int indirect;
    unsigned long file_size;
 };
 ```
This `struct_inode_info` is the same for both regular file and directory.

## How to use it
### CAVEAT: you may want to use a virtual machine to do the following in case this filesystem module harm you system

Things are similar in many linux distribution, I would use Ubuntu/Debian as example:
  Get the kernel header. Simply download the kernel header
  
      `username@machine:~/sfs$ sudo apt-get install linux-headers-`uname -r`
  
  (you can also use some specific version of kernel and change the makefile. For example:
  
      `git clone git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git`
      `cd linux`
      `git checkout v2.6.36.2`
      
  and then change in the `Makefile' the `/lib/modules/$(shell uname -r)/build' to the new kernel directory. )
  
  And also make sure you have all the required dev package:
      
      `username@machine:~/sfs$ sudo apt-get install gcc build-essential`
      
  Then in the source dir, you can compile the sfs filesystem module:
  
      `username@machine:~/sfs$ make`
  
  If the weather is good enough then it would compile successfully. But as the linux kernel evovle, with its internal API
  change frequently, sometimes things would break. You have to manualy fix it. Usually you just have to change a few lines.
  Note that if you see warning, you can ignore it.
  
  Then install the module:
  
      `username@machine:~/sfs$ sudo insmod sfs.ko`
      
  and format the image file(we use a normal file as disk, with help from linux's loop device mechanism):
  
      `username@machine:~/sfs$ mksfs.sfs ./image`
  
  mount the image:
  
      `username@machine:~/sfs$ sudo mount -o loop -t sfs ./image ./dir`
      
  to make things smooth, you may want to change the permission of the mounted directory:
  
      `username@machine:~/sfs$ sudo chown username:usergroup ./dir`
      
  then a simple filesystem is just under `./dir`. Walk into it and make some change:
  
      `username@machine:~/sfs$ cd ./dir`
      `username@machine:~/sfs$ touch lala.txt`
      `username@machine:~/sfs$ echo "Hello World" > lala.txt`
      `username@machine:~/sfs$ cat lala.txt`
      `username@machine:~/sfs$ rm lala.txt`
      `username@machine:~/sfs$ mkdir newdir`
      `username@machine:~/sfs$ cd newdir`
      ...
  
  That's it.
  Note that the filesystem sometimes might crash(at least in my virtual machine). I would try to make it more robust in
  the future.
  
## Issue
Though I am not targeting at a **real** filesystem, I still want to know more about it. I'm still not so clear with
some of linux's mechanism(dentry cache for example), so if you find something in my code that are doing explicitly
the wrong thing, please let me know. Really appreciate it. Thanks!

## Acknowledgement
I would like to thank @psankar's [simplefs](https://github.com/psankar/simplefs). Some ideas are borrowed from there.


## License
Creative Commons Zero License. Public domain
