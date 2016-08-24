/*
 * a clean view of all the essential struct of Linux VFS
 */

/* ======= struct in current kernel version ========= */
struct super_block {
    struct                          list_head s_list;  /* Keep this first */
    dev_t                           s_dev;             /* search index; _not_ kdev_t */
    unsigned char                   s_blocksize_bits;
    unsigned long                   s_blocksize;
    loff_t                          s_maxbytes;        /* Max file size */
    struct file_system_type        *s_type;
    const struct super_operations  *s_op;
    const struct dquot_operations  *dq_op;
    const struct quotactl_ops      *s_qcop;
    const struct export_operations *s_export_op;
    unsigned long                   s_flags;
    unsigned long                   s_iflags; /* internal SB_I_* flags */
    unsigned long                   s_magic;
    struct dentry                  *s_root;
    struct rw_semaphore             s_umount;
    int                             s_count;
    atomic_t                        s_active;
#ifdef CONFIG_SECURITY
    void *                          s_security;
#endif
    const struct xattr_handler    **s_xattr;

    const struct fscrypt_operations *s_cop;

    struct hlist_bl_head        s_anon; /* anonymous dentries for (nfs) exporting */
    struct list_head            s_mounts;   /* list of mounts; _not_ for fs use */
    struct block_device        *s_bdev;
    struct backing_dev_info    *s_bdi;
    struct mtd_info            *s_mtd; /* memory disk information? */
    struct hlist_node           s_instances;
    unsigned int                s_quota_types; /* Bitmask of supported quota types */
    struct quota_info           s_dquot;  /* Diskquota specific options */

    struct sb_writers           s_writers;

    char s_id[32]; /* Informational name */
    u8   s_uuid[16]; /* UUID */

    void        *s_fs_info; /* Filesystem private info */
    unsigned int s_max_links;
    fmode_t      s_mode;

    /* Granularity of c/m/atime in ns.
       Cannot be worse than a second */
    u32 s_time_gran;

    /*
     * The next field is for VFS *only*. No filesystems have any business
     * even looking at it. You had been warned.
     */
    struct mutex s_vfs_rename_mutex; /* Kludge */

    /*
     * Filesystem subtype.  If non-empty the filesystem type field
     * in /proc/mounts will be "type.subtype"
     */
    char *s_subtype;

    /*
     * Saved mount options for lazy filesystems using
     * generic_show_options()
     */
    char __rcu  *s_options;
    const struct dentry_operations *s_d_op; /* default d_op for dentries */

    /*
     * Saved pool identifier for cleancache (-1 means none)
     */
    int cleancache_poolid;

    struct shrinker s_shrink; /* per-sb shrinker handle */

    /* Number of inodes with nlink == 0 but still referenced */
    atomic_long_t s_remove_count;

    /* Being remounted read-only */
    int s_readonly_remount;

    /* AIO completions deferred from interrupt context */
    struct workqueue_struct *s_dio_done_wq;
    struct hlist_head s_pins;

    /*
     * Keep the lru lists last in the structure so they always sit on their
     * own individual cachelines.
     */
    struct list_lru s_dentry_lru ____cacheline_aligned_in_smp;
    struct list_lru s_inode_lru ____cacheline_aligned_in_smp;
    struct rcu_head rcu;
    struct work_struct destroy_work;

    struct mutex s_sync_lock; /* sync serialisation lock */

    /*
     * Indicates how deep in a filesystem stack this SB is
     */
    int s_stack_depth;

    /* s_inode_list_lock protects s_inodes */
    spinlock_t s_inode_list_lock ____cacheline_aligned_in_smp;
    struct list_head s_inodes; /* all inodes */
};

/*
 * Keep mostly read-only and often accessed (especially for
 * the RCU path lookup and 'stat' data) fields at the beginning
 * of the 'struct inode'
 */
struct inode {
    umode_t			i_mode;
    unsigned short	i_opflags;
    kuid_t			i_uid;
    kgid_t			i_gid;
    unsigned int	i_flags; /* filesystem flags */

#ifdef CONFIG_FS_POSIX_ACL
    struct posix_acl	*i_acl;
    struct posix_acl	*i_default_acl;
#endif

    const struct inode_operations *i_op;
    struct super_block	          *i_sb;
    struct address_space          *i_mapping;

#ifdef CONFIG_SECURITY
    void			*i_security;
#endif

    /* Stat data, not accessed from path walking */
    unsigned long		i_ino;
    /*
     * Filesystems may only read i_nlink directly.  They shall use the
     * following functions for modification:
     *
     *    (set|clear|inc|drop)_nlink
     *    inode_(inc|dec)_link_count
     */
    union {
        const unsigned int i_nlink;
        unsigned int __i_nlink;
    };
    dev_t	        i_rdev;
    loff_t	        i_size;
    struct timespec	i_atime;
    struct timespec	i_mtime;
    struct timespec	i_ctime;
    spinlock_t		i_lock;	/* i_blocks, i_bytes, maybe i_size */
    unsigned short  i_bytes;
    unsigned int	i_blkbits;
    blkcnt_t		i_blocks;

#ifdef __NEED_I_SIZE_ORDERED
    seqcount_t		i_size_seqcount;
#endif

    /* Misc */
    unsigned long		i_state;
    struct mutex		i_mutex;

    unsigned long		dirtied_when;	/* jiffies of first dirtying */
    unsigned long		dirtied_time_when;

    struct hlist_node	i_hash;
    struct list_head	i_io_list;	/* backing dev IO list */
#ifdef CONFIG_CGROUP_WRITEBACK
    struct bdi_writeback	*i_wb;		/* the associated cgroup wb */

    /* foreign inode detection, see wbc_detach_inode() */
    int			i_wb_frn_winner;
    u16			i_wb_frn_avg_time;
    u16			i_wb_frn_history;
#endif
    struct list_head	i_lru;		/* inode LRU list */
    struct list_head	i_sb_list;
    union {
        struct hlist_head	i_dentry;
        struct rcu_head		i_rcu;
    };
    u64			i_version;
    atomic_t		i_count;
    atomic_t		i_dio_count;
    atomic_t		i_writecount;
#ifdef CONFIG_IMA
    atomic_t		i_readcount; /* struct files open RO */
#endif
    const struct file_operations *i_fop;	/* former ->i_op->default_file_ops */
    struct file_lock_context     *i_flctx;
    struct address_space          i_data;
    struct list_head              i_devices;
    union {
        struct pipe_inode_info	*i_pipe;
        struct block_device	*i_bdev;
        struct cdev		*i_cdev;
        char			*i_link;
    };

    __u32			i_generation;

#ifdef CONFIG_FSNOTIFY
    __u32			i_fsnotify_mask; /* all events this inode cares about */
    struct hlist_head	i_fsnotify_marks;
#endif

#if IS_ENABLED(CONFIG_FS_ENCRYPTION)
    struct fscrypt_info	*i_crypt_info;
#endif

    void			*i_private; /* fs or device private pointer */
};


/* ======== old Linux struct with Robert Love's comments ====== */

struct super_block {
    struct list_head          s_list;           /* list of all superblocks */
    dev_t                     s_dev;            /* identifier */
    unsigned long             s_blocksize;      /* block size in bytes */
    unsigned char             s_blocksize_bits; /* block size in bits */
    unsigned char             s_dirt;           /* dirty flag */
    unsigned long long        s_maxbytes;       /* max file size */
    struct file_system_type   s_type;           /* filesystem type */
    struct super_operations   s_op;             /* superblock methods */
    struct dquot_operations  *dq_op;            /* quota methods */
    struct quotactl_ops      *s_qcop;           /* quota control methods */
    struct export_operations *s_export_op;      /* export methods */
    unsigned long             s_flags;          /* mount flags */
    unsigned long             s_magic;          /* filesystem’s magic number */
    struct dentry            *s_root;           /* directory mount point */
    struct rw_semaphore       s_umount;         /* unmount semaphore */
    struct semaphore          s_lock;           /* superblock semaphore */
    int                       s_count;          /* superblock ref count */
    int                       s_need_sync;      /* not-yet-synced flag */
    atomic_t                  s_active;         /* active reference count */
    void                     *s_security;       /* security module */
    struct xattr_handler    **s_xattr;          /* extended attribute handlers */
    struct list_head          s_inodes;         /* list of inodes */
    struct list_head          s_dirty;          /* list of dirty inodes */
    struct list_head          s_io;             /* list of writebacks */
    struct list head          s_more_io;        /* list of more writebacks */
    struct hlist_head         s_anon;           /* anonymous dentries */
    struct list_head          s_files;          /* list of assigned files */
    struct list_head          s_dentry_lru;     /* list of unused dentries */
    int                       s_nr_dentry_unused; /* list of dentries unused */
    struct block_device      *s_bdev;           /* associated block device */
    struct mtd_info          *s_mtd;            /* memory disk information */
    struct list_head          s_instance;       /* instance of this fs */
    struct quota_info         s_dquot;          /* quota-specific options */
    int                       s_frozen;         /* frozen status */
    wait_queue_head_t         s_wait_unfrozen;  /* wait queue on freeze */
    char                      s_id[32];         /* text name */
    void                     *s_fs_info;        /* fs-specific info */
    fmode_t                   s_mode;           /* mount permission */
    struct semaphore          s_vfs_rename_sem; /* rename semaphore */
    u32                       s_time_gran;      /* granularity of times stamps */
    char                     *sub_type;         /* subtype name */
    char                     *s_options;        /* save mounted options */
};

struct super_operations {
    /* ... */
};

struct inode {
    struct hlist_node        i_hash;             /* hash list */
    struct list_head         i_list;             /* list of inodes */
    struct list_head         i_sb_list;          /* list of superblocks */
    struct list_head         i_dentry;           /* list of dentries */
    unsigned long            i_ino;              /* inode number */
    atomic_t                 i_count;            /* reference counter */
    unsigned int             i_nlink;            /* number of hard links */
    uid_t                    i_uid;              /* user id of owner */
    gid_t                    i_gid;              /* group id of owner */
    kdev_t                   i_rdev;             /* real device node */
    u64                      i_version;          /* versioning number */
    loff_t                   i_size;             /* file size in bytes */
    seqcount_t               i_size_seqcount;    /* serializer for i_size */
    struct timespec          i_atime;            /* last access time */
    struct timespec          i_mtime;            /* last modify time */
    struct timespec          i_ctime;            /* last change time */
    unsigned int             i_blkbits;          /* block size in bits */
    blkcnt_t                 i_blocks;           /* file size in blocks */
    unsigned short           i_bytes;            /* bytes consumed */
    umode_t                  i_mode;             /* access permissions */
    spinlock_t               i_lock;             /* spinlock */
    struct rw_semaphore      i_alloc_sem;        /* nests inside of i_sem */
    struct semaphore         i_sem;              /* inode semaphore */
    struct inode_operations *i_op;               /* inode ops table */
    struct file_operations  *i_fop;              /* default inode ops */
    struct super_block      *i_sb;               /* associated superblock */
    struct file_lock        *i_flock;            /* file lock list */
    struct address_space    *i_mapping;          /* associated mapping */
    struct address_space     i_data;             /* mapping for device */
    struct dquot            *i_dquot[MAXQUOTAS]; /* disk quotas for inode */
    struct list_head         i_devices;          /* list of block devices */
    union {
        struct pipe_inode_info *i_pipe;   /* pipe information */
        struct block_device    *i_bdev;   /* block device driver */
        struct cdev            *i_cdev;   /* character device driver */
    };
    unsigned long          i_dnotify_mask;   /* directory notify mask */
    struct dnotify_struct *i_dnotify;        /* dnotify */
    struct list_head       inotify_watches;  /* inotify watches */
    struct mutex           inotify_mutex;    /* protects inotify_watches */
    unsigned long          i_state;          /* state flags */
    unsigned long          dirtied_when;     /* first dirtying time */
    unsigned int           i_flags;          /* filesystem flags */
    atomic_t               i_writecount;     /* count of writers */
    void                  *i_security;       /* security module */
    void                  *i_private;        /* fs private pointer */
};

struct inode_operations {
    /* ... */
};

struct dentry {
    atomic_t                   d_count;   /* usage count */
    unsigned int               d_flags;   /* dentry flags */
    spinlock_t                 d_lock;    /* per-dentry lock */
    int                        d_mounted; /* is this a mount point? */
    struct inode              *d_inode;  /* associated inode */
    struct hlist_node          d_hash;    /* list of hash table entries */
    struct dentry             *d_parent; /* dentry object of parent */
    struct qstr                d_name;    /* dentry name */
    struct list_head           d_lru;     /* unused list */
    union {
        struct list_head       d_child;  /* list of dentries within */
        struct rcu_head        d_rcu; /* RCU locking */
    } d_u;                  
    struct list_head           d_subdirs; /* subdirectories */
    struct list_head           d_alias; /* list of alias inodes */
    unsigned long              d_time; /* revalidate time */
    struct dentry_operations  *d_op; /* dentry operations table */
    struct super_block        *d_sb; /* superblock of file */
    void                      *d_fsdata; /* filesystem-specific data */
    unsigned char              d_iname[DNAME_INLINE_LEN_MIN]; /* short name */
};

struct dentry_operations {
    int (*d_revalidate) (struct dentry *, struct nameidata *);
    int (*d_hash) (struct dentry *, struct qstr *);
    int (*d_compare) (struct dentry *, struct qstr *, struct qstr *);
    int (*d_delete) (struct dentry *);
    void (*d_release) (struct dentry *);
    void (*d_input) (struct dentry *, struct inode *);
    char *(*d_name) (struct dentry *, char *, int);
};

struct file {
    union {
        struct list_head       fu_list;          /* list of file objects */
        struct rcu_head        fu_rcuhead;       /* RCU list after freeing */
    } f_u;                 
    struct path                f_path;            /* contains the dentry */
    struct file_operations    *f_op;              /* file operations table */
    spinlock_t                 f_lock;            /* per-file struct lock */
    atomic_t                   f_count;           /* file object’s usage count */
    unsigned int               f_flags;           /* flags specified on open */
    mode_t                     f_mode;            /* file access mode */
    loff_t                     f_pos;             /* file offset (file pointer) */
    struct fown_struct         f_owner;           /* owner data for signals */
    const struct cred         *f_cred;            /* file credentials */
    struct file_ra_state       f_ra;              /* read-ahead state */
    u64                        f_version;         /* version number */
    void                      *f_security;        /* security module */
    void                      *private_data;      /* tty driver hook */
    struct list_head           f_ep_links;        /* list of epoll links */
    spinlock_t                 f_ep_lock;         /* epoll lock */
    struct address_space      *f_mapping;         /* page cache mapping */
    unsigned long              f_mnt_write_state; /* debugging state */
};

struct file_operations {
    /* .... */
};
