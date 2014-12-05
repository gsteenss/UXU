
/* 
   Uzix X-Utils (cross platform utilities)   
   (C) 2003 Felipe Bergo - bergo@seul.org 

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License,
   version 2 or (at your option) any later version. The license
   is included in the COPYING file.
*/

/* most info was taken from the UZIX source headers */

#ifndef UZIXFS_H
#define UZIXFS_H 1

/* FILE *f */
#include <stdio.h>

/* ISO C9X data types */
#include <stdint.h>

#define UZ_DBLOCKS  18
#define UZ_IBLOCKS  1
#define UZ_DIBLOCKS 1
#define UZ_BPI      20 /* blocks per inode */

#define UZ_SBLOCK      1   /* disk block of filesystem superblock */
#define UZ_ROOT        1   /* Inode # of / for all mounted filesystems */

#define UZ_BLOCKSZ     512
#define UZ_BLOCKSZ_L2  9

#define UZ_ILEN        64
#define UZ_IPB         8       /* # of inodes per logical block */
#define UZ_IPB_L2      3       /* log2(UZ_IPB) */
#define UZ_IPB_MASK    ((1<<UZ_IPB_L2)-1)

#define UZ_SBSIG       19638   /* superblock signature */

typedef uint16_t uz_mode_t;
typedef int32_t  uz_off_t;
typedef uint16_t uz_blkno_t;
typedef uint16_t uz_ino_t;

typedef struct {
        uint16_t  t_time;
        uint16_t  t_date;
} uz_time_t;

typedef struct {
  uz_mode_t  i_mode;         /* file mode                off:0  len:2  */
  uint16_t   i_nlink;        /* number of links to file  off:2  len:2  */
  uint8_t    i_uid, i_gid;   /* file owner               off:4  len:2  */
  uz_off_t   i_size;         /* file size                off:6  len:4  */
  uz_time_t  i_atime;        /* last access time         off:10 len:4  */
  uz_time_t  i_mtime;        /* last modification time   off:14 len:4  */
  uz_time_t  i_ctime;        /* file creation time       off:18 len:4  */
  uz_blkno_t i_addr[20];     /* inodes themselves        off:22 len:40 */
  uz_blkno_t i_dummy;        /* 64-byte padding          off:62 len:2  */
} uz_inode; /* must be 64 bytes long */

/* note: an inode is considered free when i_mode==0 and i_nlink==0 */

/* uzix mode masks */

#define UZ_IFMT          0170000 /* file type mask */
#define UZ_IFLNK         0110000 /* symbolic link */
#define UZ_IFREG         0100000 /* or just 000000, regular */
#define UZ_IFBLK         0060000 /* block special */
#define UZ_IFDIR         0040000 /* directory */
#define UZ_IFCHR         0020000 /* character special */
#define UZ_IFPIPE        0010000 /* pipe */

#define UZ_ISUID         04000   /* set euid to file uid */
#define UZ_ISGID         02000   /* set egid to file gid */
#define UZ_ISVTX         01000   /* */

#define UZ_IREAD         0400    /* owner may read */
#define UZ_IWRITE        0200    /* owner may write */
#define UZ_IEXEC         0100    /* owner may execute <directory search> */

#define UZ_IGREAD        0040    /* group may read */
#define UZ_IGWRITE       0020    /* group may write */
#define UZ_IGEXEC        0010    /* group may execute <directory search> */

#define UZ_IOREAD        0004    /* other may read */
#define UZ_IOWRITE       0002    /* other may write */
#define UZ_IOEXEC        0001    /* other may execute <directory search> */


/* device-resident super-block */
typedef struct {
  uint16_t    s_mounted;      /* signature */
  uint16_t    s_reserv;       /* # of first block of inodes */
  uint16_t    s_isize;        /* # of inode's blocks */
  uint16_t    s_fsize;        /* # of data's blocks */

  uz_blkno_t  s_tfree;        /* total free blocks */
  uint16_t    s_nfree;        /* # of free blocks in s_free */
  uz_blkno_t  s_free[50];     /* #s of free block's */

  uz_ino_t    s_tinode;       /* total free inodes */
  uint16_t    s_ninode;       /* # of free inodes in s_inode */
  uz_ino_t    s_inode[50];    /* #s of free inodes */

  uz_time_t   s_time;         /* last modification timestamp */
} uz_sblock;

#define UZ_DIRNAMELEN    14
#define UZ_DIRELEN       16

/* device directory entry */
typedef struct {
        uint16_t d_ino;               /* file's inode */
        uint8_t  d_name[UZ_DIRNAMELEN];  /* file name */
} uz_direntry;

/* all functions return 0 in case of success, -1 on error */

int uz_read_sblock(FILE *f, uz_sblock *sb);
int uz_read_inode(FILE *f, uz_sblock *sb, uz_ino_t no, uz_inode *inode);
int uz_read_data(FILE *f, uz_inode *inode, 
		 uint32_t offset, uint32_t length, void *dest);
int uz_read_raw_block(FILE *f, uz_blkno_t block, void *dest);

int uz_xlate_block(FILE *f, uz_inode *inode, int rank);
int uz_set_nth_block(FILE *f, uz_inode *inode, int rank, uz_blkno_t block);

int uz_write_sblock(FILE *f, uz_sblock *sb);
int uz_write_inode(FILE *f, uz_sblock *sb, uz_ino_t no, uz_inode *inode);
int uz_write_data(FILE *f, uz_inode *inode, 
		  uint32_t offset, uint32_t length, void *src);
int uz_write_raw_block(FILE *f, uz_blkno_t block, void *src);

int uz_alloc_inode(FILE *f, uz_sblock *sb);
int uz_free_inode(FILE *f, uz_sblock *sb, uz_ino_t inode);

int uz_alloc_block(FILE *f, uz_sblock *sb);
int uz_free_block(FILE *f, uz_sblock *sb, uz_blkno_t block);

int uz_inode_grow(FILE *f, uz_sblock *sb, uz_ino_t inode, uz_off_t length);
int uz_inode_implode(FILE *f, uz_sblock *sb, uz_ino_t inode);

int uz_inode_remove(FILE *f, uz_sblock *sb, uz_ino_t inode);

uz_blkno_t uz_fit_bytes(uz_off_t length);

/* converts uzix date/time to human-readable string */
char * uz_date_for_humans(uz_time_t *t, char *dest);
void uz_set_date(int day,int month,int year, uz_time_t *t);
void uz_set_time(int hour,int min,int sec, uz_time_t *t);
void uz_time(uz_time_t *t);

#endif

// additional stuff copied from uzix headers

/*      UZIX time format (very similar to msdos)
 *
 *      time_t.date:
 *
 *      |7|6|5|4|3|2|1|0|7|6|5|4|3|2|1|0|
 *      | | | | | | | | | | | | | | | | |
 *      \   7 bits    /\4 bits/\ 5 bits /
 *         year+1980     month     day
 *
 *      time_t.time:
 *
 *      |7|6|5|4|3|2|1|0|7|6|5|4|3|2|1|0|
 *      | | | | | | | | | | | | | | | | |
 *      \  5 bits /\  6 bits  /\ 5 bits /
 *         hour      minutes     sec*2
 */
