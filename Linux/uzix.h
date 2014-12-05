
#ifndef UZIX_H
#define UZIX_H

#include <linux/types.h>
#include <linux/fs.h>

#define UZIX_BLOCK_SIZE        512
#define UZIX_MAGIC             19638
#define UZIX_ROOT_INO          1
#define UZIX_INODE_SIZE        64
#define UZIX_IPB               8      /* inodes per block */
#define UZIX_DPB               32     /* dir entries per block */
#define UZIX_MAXNAME           13

#define UZIX_DBLOCK(i,j) (((__u16 *)((void*)&(i->u.minix_i.u.i2_data[0])))[j])
#define UZIX_SIBLOCK(i) (UZIX_DBLOCK(i,18))
#define UZIX_DIBLOCK(i) (UZIX_DBLOCK(i,19))

/* erm.... */
#define JAN_1_1980             315532800

#define PACK  __attribute__ ((packed))

/* device-resident super-block */
struct uzix_super_block {
  __u16       s_sig           PACK; 
  __u16       s_reserv        PACK;       /* first block of inodes  */
  __u16       s_isize         PACK;       /* count of inode blocks  */
  __u16       s_fsize         PACK;       /* total number of blocks */

  __u16       s_tfree         PACK;       /* # of free blocks       */
  __u16       s_nfree         PACK;       /* item count for s_free  */
  __u16       s_free[50]      PACK;       /* free block cache       */

  __u16       s_tinode        PACK;       /* # of free inodes       */
  __u16       s_ninode        PACK;       /* item count for s_inode */
  __u16       s_inode[50]     PACK;       /* free inode cache       */

  __u16       s_time_time     PACK;       /* timestamp: ho[5]:mi[6]:se[5]  */
  __u16       s_time_date     PACK;       /* timestamp: ye[7]:mo[4]:da[5] */
};

/* uzix inode */
struct uzix_inode {
  __u16      i_mode         PACK;        /* 0 */
  __u16      i_nlink        PACK;        /* 2 */
  __u8       i_uid          PACK;        /* 4 */
  __u8       i_gid          PACK;        /* 5 */
  __s32      i_size         PACK;        /* 6 */
  __u16      i_atime_time   PACK;        /* 10 */
  __u16      i_atime_date   PACK;        /* 12 */
  __u16      i_mtime_time   PACK;        /* 14 */
  __u16      i_mtime_date   PACK;        /* 16 */
  __u16      i_ctime_time   PACK;        /* 18 */
  __u16      i_ctime_date   PACK;        /* 20 */
  __u16      i_addr[20]     PACK;        /* 22 */
  __u16      i_dummy        PACK;        /* 62 */
};

/* uzix directory entry */
struct uzix_dentry {
  __u16      d_ino          PACK;
  __u8       d_name[14]     PACK;
};

struct uzix_xblock {
  __u32        blk;
  __u32        offset;
};

struct uzix_trans {
  __u16        devblksz;
  __u16        fsblksz;
};

typedef struct super_block lnx_sblock;
typedef struct inode       lnx_inode;
typedef struct dentry      lnx_dentry;

typedef struct uzix_super_block uz_sblock;
typedef struct uzix_inode       uz_inode;
typedef struct uzix_xblock      uz_xblock;
typedef struct uzix_trans       uz_trans;
typedef struct uzix_dentry      uz_dentry;

#endif
