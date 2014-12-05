/*
   UZIX filesystem support for Linux
   (C) 2003 Felipe Bergo <bergo@seul.org>

   revisions: 
              2003.01.18 - first public release <FB>

   based on Adriano C.R. Cunha's sources to the Uzix operating
   system, and the Linux drivers for Minix, FAT, RAMfs and ROMfs
   filesystems. Contains code snippets from the Linux FAT driver
   (for timestamp conversion)

   License: GNU General Public License, v2 or later
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/time.h>
#include <linux/locks.h>
#include <linux/smp_lock.h>
#include <linux/string.h>
#include <asm/byteorder.h>
#include "uzix.h"

/* declarations */

static uz_sblock *  uzix_xlate_sb(lnx_sblock *sb);
static void         uzix_xlate_block(int blk, uz_xblock *out, uz_trans *in);

static lnx_sblock * uzix_read_super(lnx_sblock *s, void *data, int silent);
static void         uzix_tconv_touzix(time_t x, __u16 *d, __u16 *t);
static time_t       uzix_tconv_tounix(__u16 d, __u16 t);

static void         uzix_read_inode(lnx_inode * inode);
static void         uzix_write_inode(lnx_inode * inode, int wait);
static void         uzix_delete_inode(lnx_inode *inode);

static void         uzix_put_super(lnx_sblock *sb);
static void         uzix_write_super(lnx_sblock * sb);
static int          uzix_statfs(lnx_sblock *sb, struct statfs *buf);
static int          uzix_remount(lnx_sblock * sb, int * flags, char * data);

static int          uzix_read_block(lnx_sblock *sb, __u16 blk, void *buf);
static int          uzix_write_block(lnx_sblock *sb, __u16 blk, void *buf);
static __u16        uzix_fit_bytes(__s32 size);

static int          uzix_free_block(lnx_sblock *sb, __u16 blk);
static int          uzix_alloc_block(lnx_sblock *sb);
static int          uzix_free_inode(lnx_inode *inode);
static int          uzix_alloc_inode(lnx_sblock *sb);
static int          uzix_block_by_rank(lnx_inode *inode, int rank);

static void         uzix_implode_inode(lnx_inode *inode);
static int          uzix_readdir(struct file *filp, void *dirent, filldir_t filldir);

static void         uzix_set_inode(lnx_inode *inode);
static int          uzix_mknod(lnx_inode * dir, lnx_dentry *dentry, int mode, int rdev);
static int          uzix_mkdir(lnx_inode * dir, lnx_dentry *dentry, int mode);
static int          uzix_rmdir(lnx_inode * dir, lnx_dentry *dentry);
static int          uzix_create(lnx_inode * dir, lnx_dentry *dentry, int mode);
static int          uzix__link(lnx_dentry *dentry, lnx_inode *inode);
static int          uzix__count(lnx_inode *inode);

static int          uzix_sync_file(struct file *file, lnx_dentry *dentry, int datasync);

static int          uzix_page_rw(struct page * page, int writing, int unlock);
static int          uzix_readpage(struct file *file, struct page * page);
static int          uzix_writepage(struct page * page);
static int          uzix_prepare_write(struct file *file, struct page *page,
				       unsigned offset, unsigned to);
static int          uzix_commit_write(struct file *file, struct page *page, 
				      unsigned offset, unsigned to);
static int          uzix_inode_grow(lnx_inode *inode, unsigned newlen);

static lnx_dentry * uzix_lookup(lnx_inode * dir, lnx_dentry *dentry);
static ino_t        uzix_inode_of(lnx_dentry *dentry);

static int          uzix_link(lnx_dentry * old_dentry, lnx_inode * dir,
			      lnx_dentry *dentry);
static int          uzix_unlink(lnx_inode * dir, lnx_dentry *dentry);
static int          uzix__rmentry(lnx_inode * dir, lnx_dentry *dentry);
static int          uzix__UGLY_free_last_block(lnx_inode *inode);

static int          uzix_rename(lnx_inode * old_dir, lnx_dentry * old_dentry,
				lnx_inode * new_dir, lnx_dentry * new_dentry);

static int          uzix_symlink(lnx_inode * dir, lnx_dentry *dentry,
				 const char * symname);

static int uzix_day_n[] = { 0,31,59,90,120,151,181,212,243,273,304,334,0,0,0,0 };
extern struct timezone sys_tz;

/* operation blocks */

static struct super_operations uzix_sops = {
	read_inode:	uzix_read_inode,
	write_inode:	uzix_write_inode,
	delete_inode:	uzix_delete_inode,
	put_super:	uzix_put_super,
	write_super:	uzix_write_super,
	statfs:		uzix_statfs,
	remount_fs:	uzix_remount,
};

struct file_operations uzix_file_operations = {
        llseek:         generic_file_llseek,
        read:           generic_file_read,
        write:          generic_file_write,
        mmap:           generic_file_mmap,
        fsync:          uzix_sync_file,
};

struct inode_operations uzix_file_inode_operations = {
        truncate:       uzix_implode_inode,
};

struct file_operations uzix_dir_operations = {
        read:           generic_read_dir,
        readdir:        uzix_readdir,
        fsync:          uzix_sync_file,
};

struct inode_operations uzix_dir_inode_operations = {
        create:         uzix_create,
        lookup:         uzix_lookup,
        link:           uzix_link,
        unlink:         uzix_unlink,
	symlink:        uzix_symlink,
        mkdir:          uzix_mkdir,
        rmdir:          uzix_rmdir,
        mknod:          uzix_mknod,
        rename:         uzix_rename,
};

static struct address_space_operations uzix_aops = {
        readpage:      uzix_readpage,
        writepage:     uzix_writepage,
	prepare_write: uzix_prepare_write,
	commit_write:  uzix_commit_write
};


// finished:   ok
// endianness: n/a
static uz_sblock *  uzix_xlate_sb(lnx_sblock *sb) {
  struct buffer_head *bh = (struct buffer_head *) (sb->u.generic_sbp);
  uz_xblock ba;
  uz_trans  bt;
  uz_sblock *usb;
  bt.devblksz = BLOCK_SIZE;
  bt.fsblksz  = UZIX_BLOCK_SIZE;
  uzix_xlate_block(1,&ba,&bt);
  usb = (uz_sblock *) &(bh->b_data[ba.offset]);
  return usb;
}

// finished:   ok
// endianness: n/a
static void uzix_xlate_block(int blk, uz_xblock *out, uz_trans *in)
{
  int factor;

  if (in->devblksz == in->fsblksz) {
    out->blk    = blk;
    out->offset = 0;
  } else if (in->devblksz > in->fsblksz) {
    factor = in->devblksz / in->fsblksz;
    out->blk    = blk / factor;
    out->offset = in->fsblksz * ( blk % factor );
  } else {
    out->blk = blk;
    out->offset = 0;
    printk("UZIX-fs: warning: devblk < fsblk, this isn't going to work.\n");
  }
}

// finished  : ok
// endianness: n/a [caller should treat it]
static void uzix_tconv_touzix(time_t x, __u16 *d, __u16 *t) {
  int day,year,nl_day,month;

  x -= sys_tz.tz_minuteswest*60;
  
  /* Jan 1 GMT 00:00:00 1980 */
  if (x < JAN_1_1980)
    x = JAN_1_1980;
  
  *t = (x % 60)/2+(((x/60) % 60) << 5)+(((x/3600) % 24) << 11);
  day = x/86400-3652;
  year = day/365;
  if ((year+3)/4+365*year > day) year--;
  day -= (year+3)/4+365*year;
  if (day == 59 && !(year & 3)) {
    nl_day = day;
    month = 2;
  }
  else {
    nl_day = (year & 3) || day <= 59 ? day : day-1;
    for (month = 0; month < 12; month++)
      if (uzix_day_n[month] > nl_day) break;
  }
  *d = nl_day-uzix_day_n[month-1]+1+(month << 5)+(year << 9);
}

// finished:   ok
// endianness: n/a
static time_t uzix_tconv_tounix(__u16 d, __u16 t) 
{
  int day, month, year;
  int hour, min, sec;

  day   = d & 0x1f; if (day==0) day=1;
  month = (d>>5)&0x0f; if (month==0) month=1;
  year  = (d>>9)&0x7f; year+=1980;

  sec    = (t<<1) & 0x3f;
  min    = (t>>5) & 0x3f;
  hour   = (t>>11) & 0x1f;

  return(mktime(year,month,day,hour,min,sec));
}

// finished:   ok
// endianness: ok
static lnx_sblock * uzix_read_super(lnx_sblock *s, void *data, int silent)
{
  struct buffer_head *bh;
  uz_sblock *ms;
  uz_xblock ba;
  uz_trans  bt;

  lnx_inode *root_inode;

  kdev_t dev = s->s_dev;
  unsigned int hblock;

  #ifdef UZIX_DEBUG
    printk("uzix: uzix_read_super\n");
  #endif

  hblock = get_hardsect_size(dev);
  
  if (hblock > BLOCK_SIZE)
    goto out_bad_hblock;

  set_blocksize(dev, BLOCK_SIZE);
  s->s_blocksize = BLOCK_SIZE;
  s->s_blocksize_bits = BLOCK_SIZE_BITS;
  bt.devblksz = BLOCK_SIZE;
  bt.fsblksz  = UZIX_BLOCK_SIZE;

  uzix_xlate_block(1,&ba,&bt);

  if (!(bh = sb_bread(s, ba.blk)))
    goto out_bad_sb;

  ms = (uz_sblock *) &(bh->b_data[ba.offset]);

  s->s_magic    = le16_to_cpu(ms->s_sig);
  s->s_maxbytes = 512 * 65535; /* unreachable theoretical limit */

  if (s->s_magic != UZIX_MAGIC)
    goto out_no_fs;

  s->u.generic_sbp = (void *) bh;
  s->s_op          = &uzix_sops;

  root_inode = iget(s, UZIX_ROOT_INO);
  if (!root_inode || is_bad_inode(root_inode))
    goto out_no_root;

  s->s_root = d_alloc_root(root_inode);
  if (!s->s_root)
    goto out_iput;

  /* s->s_root->d_op = &minix_dentry_operations; */

  return s;

 out_iput:
  iput(root_inode);
  goto out_free;

 out_no_root:
  printk("UZIX-fs: get root inode failed\n");
  goto out_free;

 out_no_fs:
  printk("UZIX-fs: bad superblock magic number.\n");
  goto out_free;

  // out_incomplete:
 out_free:
  brelse(bh);
  return NULL;

 out_bad_hblock:
  printk("UZIX-fs: blocksize too small for device.\n");
  goto out;

 out_bad_sb:
  printk("UZIX-fs: unable to read superblock\n");

 out:
  return NULL;
}

// finished  : ok
// endianness: ok
static void uzix_read_inode(lnx_inode * inode)
{
  uz_sblock *usb;
  uz_inode  iblk[UZIX_IPB], *raw;
  int blk, ioff, i;

  #ifdef UZIX_DEBUG
    printk("uzix: uzix_read_inode (#%lu)\n",inode->i_ino);
  #endif

  usb = uzix_xlate_sb(inode->i_sb);

  blk  = le16_to_cpu(usb->s_reserv) + (inode->i_ino / UZIX_IPB);
  ioff = inode->i_ino % UZIX_IPB;

#ifdef UZIX_DEBUG
  printk("uzix: ** blk=%d off=%d sizeof(uz_inode)=%d\n",blk,ioff,sizeof(uz_inode));
#endif

  if (uzix_read_block(inode->i_sb, blk, (void *) iblk)!=0)
    goto out_bad;

  raw = &iblk[ioff];

  inode->i_mode     = le16_to_cpu(raw->i_mode);
  inode->i_nlink    = le16_to_cpu(raw->i_nlink);
  inode->i_uid      = (uid_t) (raw->i_uid);
  inode->i_gid      = (gid_t) (raw->i_gid);
  inode->i_size     = le32_to_cpu(raw->i_size);
  inode->i_blocks   = 0;
  inode->i_blksize  = 0;
  inode->i_atime      = uzix_tconv_tounix(le16_to_cpu(raw->i_atime_date),
					  le16_to_cpu(raw->i_atime_time));
  inode->i_mtime      = uzix_tconv_tounix(le16_to_cpu(raw->i_mtime_date),
					  le16_to_cpu(raw->i_mtime_time));
  inode->i_ctime      = uzix_tconv_tounix(le16_to_cpu(raw->i_ctime_date),
					  le16_to_cpu(raw->i_ctime_time));

  for(i=0;i<20;i++)
    UZIX_DBLOCK(inode,i) = le16_to_cpu(raw->i_addr[i]);

  uzix_set_inode(inode);
  return;

 out_bad:
  make_bad_inode(inode);
  return;
}

// finished  : ok
// endianness: ok
static void uzix_write_inode(lnx_inode * inode, int wait)
{
  uz_sblock *usb;
  uz_xblock ba;
  uz_trans  bt;
  uz_inode  *raw;
  struct buffer_head *bh;
  int blk, ioff, i;
  __u16 d,t;

  #ifdef UZIX_DEBUG
    printk("uzix: uzix_write_inode (#%lu)\n",inode->i_ino);
  #endif

  lock_kernel();

  bt.devblksz = BLOCK_SIZE;
  bt.fsblksz  = UZIX_BLOCK_SIZE;

  usb = uzix_xlate_sb(inode->i_sb);

  blk  = le16_to_cpu(usb->s_reserv) + (inode->i_ino / UZIX_IPB);
  ioff = UZIX_INODE_SIZE * (inode->i_ino % UZIX_IPB);

  uzix_xlate_block(blk,&ba,&bt);
  bh = sb_bread(inode->i_sb, ba.blk);
  if (!bh) goto out_bad;
  
  raw = (uz_inode *) (&(bh->b_data[ba.offset + ioff]));

  raw->i_mode       = cpu_to_le16(inode->i_mode);
  raw->i_nlink      = cpu_to_le16(inode->i_nlink);
  raw->i_uid        = (inode->i_uid > 255) ? 255 : (__u8) (inode->i_uid);
  raw->i_gid        = (inode->i_gid > 255) ? 255 : (__u8) (inode->i_gid);
  raw->i_size       = cpu_to_le32(inode->i_size);

  uzix_tconv_touzix(inode->i_atime, &d, &t);
  raw->i_atime_date = cpu_to_le16(d);
  raw->i_atime_time = cpu_to_le16(t);

  uzix_tconv_touzix(inode->i_mtime, &d, &t);
  raw->i_mtime_date = cpu_to_le16(d);
  raw->i_mtime_time = cpu_to_le16(t);

  uzix_tconv_touzix(inode->i_ctime, &d, &t);
  raw->i_ctime_date = cpu_to_le16(d);
  raw->i_ctime_time = cpu_to_le16(t);

  for(i=0;i<20;i++)
    raw->i_addr[i] = cpu_to_le16(UZIX_DBLOCK(inode,i));

  mark_buffer_dirty(bh);
  unlock_kernel();
  brelse(bh);

  return;

 out_bad:
  make_bad_inode(inode);
  unlock_kernel();
  return;
}

// finished  : no
// endianness: n/a
static void uzix_delete_inode(lnx_inode *inode)
{
  #ifdef UZIX_DEBUG
    printk("uzix: uzix_delete_inode (#%lu)\n",inode->i_ino);
  #endif

  lock_kernel();
  uzix_implode_inode(inode);
  uzix_free_inode(inode);
  clear_inode(inode);
  unlock_kernel();
}

// finished  : ok
// endianness: n/a
static void uzix_put_super(lnx_sblock *sb)
{
  #ifdef UZIX_DEBUG
    printk("uzix: uzix_put_super\n");
  #endif

  if (!(sb->s_flags & MS_RDONLY))
    mark_buffer_dirty((struct buffer_head *)(sb->u.generic_sbp));
  brelse((struct buffer_head *)(sb->u.generic_sbp));
  return;
}

// finished  : ok
// endianness: n/a
static void uzix_write_super(lnx_sblock * sb)
{
  #ifdef UZIX_DEBUG
    printk("uzix: uzix_write_super\n");
  #endif
  if (!(sb->s_flags & MS_RDONLY))
    mark_buffer_dirty((struct buffer_head *)(sb->u.generic_sbp));
  sb->s_dirt = 0;
}

// finished  : ok
// endianness: ok
static int uzix_statfs(lnx_sblock *sb, struct statfs *buf)
{
  uz_sblock *usb;

  #ifdef UZIX_DEBUG
    printk("uzix: uzix_statfs\n");
  #endif

  usb = uzix_xlate_sb(sb);

  buf->f_type    = sb->s_magic;
  buf->f_bsize   = sb->s_blocksize;
  buf->f_blocks  = (le16_to_cpu(usb->s_fsize)+1) / 2;
  buf->f_bfree   = (le16_to_cpu(usb->s_tfree)+1) / 2;
  buf->f_bavail  = (le16_to_cpu(usb->s_tfree)+1) / 2;
  buf->f_files   = le16_to_cpu(usb->s_isize) * UZIX_IPB;
  buf->f_ffree   = le16_to_cpu(usb->s_tinode);
  buf->f_namelen = UZIX_MAXNAME; /* most likely uzix will break if 14th char isn't 0 */

  return 0;
}

// finished  : ok
// endianness: n/a
static int uzix_remount(lnx_sblock * sb, int * flags, char * data)
{
  #ifdef UZIX_DEBUG
    printk("uzix: uzix_remount\n");
  #endif
  if ((*flags & MS_RDONLY) == (sb->s_flags & MS_RDONLY))
    return 0;
  if (*flags & MS_RDONLY) {
    mark_buffer_dirty((struct buffer_head *)(sb->u.generic_sbp));
    sb->s_dirt = 0;
  }
  return 0;
}

// finished  : ok
// endianness: n/a (block is byte-oriented)
static int  uzix_read_block(lnx_sblock *sb, __u16 blk, void *buf)
{
  struct buffer_head *bh;
  uz_xblock ba;
  uz_trans  bt;

  #if UZIX_DEBUG >= 2
    printk("uzix: uzix_read_block #%d\n",blk);
  #endif

  bt.devblksz = BLOCK_SIZE;
  bt.fsblksz  = UZIX_BLOCK_SIZE;

  uzix_xlate_block(blk,&ba,&bt);
  
  bh = sb_bread(sb, ba.blk);
  if (!bh) return -1;
  
  memcpy(buf, &(bh->b_data[ba.offset]), UZIX_BLOCK_SIZE);
  brelse(bh);
  return 0;
}

// finished  : ok
// endianness: n/a (block is byte-oriented)
static int uzix_write_block(lnx_sblock *sb, __u16 blk, void *buf)
{
  struct buffer_head *bh;
  uz_xblock ba;
  uz_trans  bt;

  #if UZIX_DEBUG >= 2
    printk("uzix: uzix_write_block #%d\n",blk);
  #endif

  bt.devblksz = BLOCK_SIZE;
  bt.fsblksz  = UZIX_BLOCK_SIZE;

  uzix_xlate_block(blk,&ba,&bt);
  
  bh = sb_bread(sb, ba.blk);
  if (!bh) return -1;
  
  memcpy(&(bh->b_data[ba.offset]), buf, UZIX_BLOCK_SIZE);
  mark_buffer_dirty(bh);
  brelse(bh);
  return 0;
}

// finished  : ok
// endianness: n/a
static __u16 uzix_fit_bytes(__s32 size) {
  __u16 x;
  x = (size / UZIX_BLOCK_SIZE);
  if (size % UZIX_BLOCK_SIZE) ++x;
  return x;
}

// finished  : ok
// endianness: ok
static int uzix_free_block(lnx_sblock *sb, __u16 blk) {
  uz_sblock *usb;
  __u16 bc[256];
  int i;

#ifdef UZIX_DEBUG
  printk("uzix: uzix_free_block #%d\n",(int)blk);
#endif

  lock_super(sb);
  usb = uzix_xlate_sb(sb);

  // flush cache to current block if it's full (the cache)
  if (usb->s_nfree == cpu_to_le16(50)) {
    memset(bc,0,UZIX_BLOCK_SIZE);
    bc[0] = usb->s_nfree;
    for(i=0;i<50;i++)
      bc[i+1] = usb->s_free[i];

    if (uzix_write_block(sb, blk, (void *)bc)!=0) return -1; 
    usb->s_nfree = cpu_to_le16(0);
  }

  usb->s_tfree = cpu_to_le16(le16_to_cpu(usb->s_tfree) + 1);  
  usb->s_free[le16_to_cpu(usb->s_nfree)] = cpu_to_le16(blk);
  usb->s_nfree = cpu_to_le16(le16_to_cpu(usb->s_nfree) + 1);  

  sb->s_dirt  = 1;
  unlock_super(sb);
  return 0;
}

// finished:   yes
// endianness: yes
static int uzix_alloc_block(lnx_sblock *sb) {
  uz_sblock *usb;
  int res = -1, newno, x;
  __u16 blk[256];

#ifdef UZIX_DEBUG
  printk("uzix: uzix_alloc_block\n");
#endif

  lock_super(sb);
  usb = uzix_xlate_sb(sb);

  /* 0 is 0 in any endianness */
  if (usb->s_nfree == 0) { res = -EIO;    goto out_err; } /* inconsistency  */
  if (usb->s_tfree == 0) { res = -ENOSPC; goto out_err; } /* fs is full :-( */

  x = le16_to_cpu(usb->s_nfree);
  newno = le16_to_cpu(usb->s_free[--x]);

  if (newno == 0) {
    res = -ENOSPC;
    goto out_err;
  }

  usb->s_nfree = cpu_to_le16(x);

  /* account for the alloc'ed block */
  x = le16_to_cpu(usb->s_tfree);
  usb->s_tfree = cpu_to_le16(x-1);

  /* refill free block cache if needed */
  if (usb->s_nfree == 0) {
    if (uzix_read_block(sb, newno, (void *) blk) != 0) {
      res = -EIO;
      goto out_err;
    }

    /* since both block data and superblock are in the
       same endianness, I can copy between them without
       conversion */

    usb->s_nfree = blk[0];

    for(x=0;x<50;x++)
      usb->s_free[x] = blk[x+1];
  }

  /* wipe out block */
  memset(blk,0,512);
  uzix_write_block(sb, newno, (void *) blk);

  sb->s_dirt = 1;
  unlock_super(sb);
  return newno;

 out_err:
  sb->s_dirt = 1;
  unlock_super(sb);
  return res;
}

// finished:   yes
// endianness: yes
static int uzix_free_inode(lnx_inode *inode) {
  lnx_sblock *sb = inode->i_sb;
  uz_sblock *usb;
  int x;

#ifdef UZIX_DEBUG
  printk("uzix: uzix_free_inode #%d\n",(int)(inode->i_ino));
#endif

  lock_super(sb);
  usb = uzix_xlate_sb(sb);

  x = le16_to_cpu(usb->s_tinode);
  usb->s_tinode = cpu_to_le16(x+1);
  
  /* if there is room, add inode to free inode cache */
  x = le16_to_cpu(usb->s_ninode);
  if (x < 50) {
    usb->s_inode[x] = cpu_to_le16((__u16)(inode->i_ino));
    usb->s_ninode = cpu_to_le16(x+1);
  }

  /* zero out inode on disk */

  inode->i_mode  = 0;
  inode->i_nlink = 0;
  inode->i_uid   = 0;
  inode->i_gid   = 0;
  inode->i_size  = 0;
  inode->i_atime = JAN_1_1980;
  inode->i_mtime = JAN_1_1980;
  inode->i_ctime = JAN_1_1980;

  for(x=0;x<20;x++)
    UZIX_DBLOCK(inode,x) = 0;

  sb->s_dirt = 1;
  unlock_super(sb);
  mark_inode_dirty(inode);
  return 0;
}

// finished:   yes
// endianness: yes
static int uzix_alloc_inode(lnx_sblock *sb) {
  uz_sblock *usb;
  int newno = -ENOSPC;
  int i, j, k, maxino, niblk, fib;
  uz_inode  buf[UZIX_IPB];

#ifdef UZIX_DEBUG
  printk("uzix: uzix_alloc_inode\n");
#endif

  lock_super(sb);
  usb = uzix_xlate_sb(sb);
  
  if (!usb->s_tinode) return -ENOSPC; /* no inodes left */
  niblk = le16_to_cpu(usb->s_isize);
  maxino = UZIX_IPB * niblk;
  fib = le16_to_cpu(usb->s_reserv);

  /* refill inode cache if empty */
  if (usb->s_ninode == 0) {
    k=0;
    for(i=0;i<niblk;i++) {
      if (uzix_read_block(sb, fib+i, (void *) buf)!=0)   
        continue;
      for(j=(i==0)?2:0;j<UZIX_IPB;j++) {
        if (buf[j].i_mode == 0 && buf[j].i_nlink == 0)
	  usb->s_inode[k++] = cpu_to_le16( (__u16) (i*UZIX_IPB + j) );
        if (k==50) goto its_full;
      }
    }
   its_full:
    usb->s_ninode = cpu_to_le16((__u16)k);
  }

  /* get an inode from the cache */
  if (usb->s_ninode) {
    i = le16_to_cpu(usb->s_ninode) - 1;
    newno = le16_to_cpu(usb->s_inode[i]);
    usb->s_ninode = cpu_to_le16((__u16)i);
    if (newno <= UZIX_ROOT_INO || newno >= maxino) {
      printk("UZIX-fs: the inode table is kaputz.\n");
      newno = -ENOSPC;
    } else {
      i = le16_to_cpu(usb->s_tinode);
      usb->s_tinode = cpu_to_le16(i-1);
    }
  }

  sb->s_dirt = 1;
  unlock_super(sb);
  return newno;
}

// finished  : ok
// endianness: ok
static int uzix_block_by_rank(lnx_inode *inode, int rank) {
  __u16 local[256];
  int x;

  // direct
  if (rank < 18)
    return(UZIX_DBLOCK(inode,rank));

  // single indirect
  if (rank >= 18 && rank < 274) {
    if (uzix_read_block(inode->i_sb,UZIX_SIBLOCK(inode),(void *)local) != 0)
      goto out_err;
    return(le16_to_cpu(local[rank-18]));
  }

  // double indirect
  if (rank >= 274 && rank <= 65809) {
    if (uzix_read_block(inode->i_sb,UZIX_DIBLOCK(inode),(void *)local) != 0)
      goto out_err;
    x = le16_to_cpu(local[ (rank - 274) / 256 ]);
    if (uzix_read_block(inode->i_sb, x ,(void *)local) != 0)
      goto out_err;
    return(le16_to_cpu(local[ (rank - 274) % 256 ]));
  }

 out_err:
  printk("UZIX-fs: block-by-rank failed, rank = %d\n",rank);
  return -1;
}

// finished  : ok
// endianness: ok
static void uzix_implode_inode(lnx_inode *inode) {
  int i,j,k;
  lnx_sblock *sb = inode->i_sb;
  __u16 x, blk[256];

  #ifdef UZIX_DEBUG
    printk("uzix: uzix_implode_inode (#%lu)\n",inode->i_ino);
  #endif

  x = uzix_fit_bytes(inode->i_size);

  // direct blocks
  for(i=0;i<x && i<18;i++) {
    if (uzix_free_block(sb, UZIX_DBLOCK(inode,i))!=0)
      goto out_err;
  }

  // single indirect
  if (x>=18) {
    if (uzix_read_block(sb, UZIX_SIBLOCK(inode), (void *)blk)!=0)
      goto out_err;
    for(i=0;i<256;i++) blk[i] = le16_to_cpu(blk[i]);
    for(i=18;i<274 && i<x;i++)
      if (uzix_free_block(sb, blk[i-18])!=0)
	goto out_err;
    if (uzix_free_block(sb, UZIX_SIBLOCK(inode))!=0)
      goto out_err;
  }

  // double indirect
  if (x>=274) {
    if (uzix_read_block(sb, UZIX_DIBLOCK(inode), (void *)blk)!=0)
      goto out_err;
    for(i=0;i<256;i++) blk[i] = le16_to_cpu(blk[i]);
    /* free leaf blocks */
    for(i=274;i<x;i++) {
      j = uzix_block_by_rank(inode, i);
      if (uzix_free_block(sb,j)!=0)
	goto out_err;
    }
    /* free middle blocks */
    for(i=274;i<x;i+=256) {
      k = (i-274) / 256;
      if (uzix_free_block(sb,blk[k])!=0)
	goto out_err;
    }
    /* free top index block */
    if (uzix_free_block(sb, UZIX_DIBLOCK(inode))!=0)
      goto out_err;
  }

  /* zero out block addresses */
  for(i=0;i<20;i++)
    UZIX_DBLOCK(inode,i) = 0;

  inode->i_size = 0;
  mark_inode_dirty(inode);
  return;

 out_err:
  printk("UZIX-fs: inode %lu nao vai funcionar(tm)\n", inode->i_ino);
  return;
}

// finished:    yes
// endianness:  yes
static int uzix_readdir(struct file *filp, void *dirent, filldir_t filldir) {
  lnx_inode *inode = filp->f_dentry->d_inode;
  lnx_sblock *sb = inode->i_sb;
  uz_dentry ue[32];
  __u16  rank, blk, poff, eno;
  int    i, over, l;
  unsigned long pos;

#ifdef UZIX_DEBUG
  printk("uzix: uzix_readdir\n");
#endif

  pos = filp->f_pos;

  for(;;) {
    eno  = pos / 16;
    rank = eno / UZIX_DPB;
    poff = eno % UZIX_DPB;
    blk  = uzix_block_by_rank(inode, rank);

#ifdef UZIX_DEBUG
    /*
    printk("uzix: fpos=%d pos=%d isize=%d\n",(int)(filp->f_pos),(int)(pos),
	   (int)(inode->i_size));
    printk("uzix: eno=%d rank=%d poff=%d\n",eno,rank,poff);
    printk("uzix: sizeof(uz_dentry)=%d\n",sizeof(uz_dentry));
    printk("uzix: %dth block is block %d\n",rank,blk);
    */
#endif

    if (!blk) goto out_nada;

    if (uzix_read_block(sb, blk, (void *) ue) != 0) {
      printk("UZIX-fs: how come!\n");
      return -EINVAL;
    }
    for(i=poff;i<32;i++) {
      if (pos >= inode->i_size)
	goto out_done;
      if (ue[i].d_ino == 0 || ue[i].d_name[0] == 0)
	goto out_done;

      pos += 16;

      l = strnlen(ue[i].d_name, UZIX_MAXNAME);
 
      ue[i].d_name[UZIX_MAXNAME] = 0;

      over = filldir(dirent, ue[i].d_name, l, 
		     rank*UZIX_BLOCK_SIZE + i*16,
		     le16_to_cpu(ue[i].d_ino), DT_UNKNOWN);
      if (over) goto out_done;
    }     
  }
  
 out_done:
  filp->f_pos = pos;
  UPDATE_ATIME(inode);
 out_nada:
  return 0;
}

// finished:   yes
// endianness: n/a
static void uzix_set_inode(struct inode *inode)
{
  inode->i_mapping->a_ops = &uzix_aops;
  if (S_ISREG(inode->i_mode)) {
    inode->i_op = &uzix_file_inode_operations;
    inode->i_fop = &uzix_file_operations;
#ifdef UZIX_DEBUG
    printk("uzix: ** regular file\n");
#endif
  } else if (S_ISDIR(inode->i_mode)) {
    inode->i_op = &uzix_dir_inode_operations;
    inode->i_fop = &uzix_dir_operations;
#ifdef UZIX_DEBUG
    printk("uzix: ** directory\n");
#endif
  } else if (S_ISLNK(inode->i_mode)) {
    inode->i_op = &page_symlink_inode_operations;
#ifdef UZIX_DEBUG
    printk("uzix: ** symlink\n");
#endif
  }
}

/* dir inode ops */

// finished:    yes
// endianness:  n/a
static int uzix_mknod(lnx_inode * dir, lnx_dentry *dentry, int mode, int rdev)
{
  lnx_sblock *sb = dir->i_sb;
  lnx_inode *inode;
  int i;

#ifdef UZIX_DEBUG
  printk("uzix: uzix_mknod\n");
#endif

  inode = new_inode(sb);
  if (!inode) return -ENOMEM;

  i = uzix_alloc_inode(sb);
  if (i<0) { iput(inode); return i; }

  inode->i_ino    = i;
  inode->i_uid    = current->fsuid;
  inode->i_gid    = (dir->i_mode & S_ISGID) ? dir->i_gid : current->fsgid;
  inode->i_mtime  = inode->i_atime = inode->i_ctime = CURRENT_TIME;
  inode->i_blocks = inode->i_blksize = 0;
  inode->i_mode   = mode;
  uzix_set_inode(inode);
  insert_inode_hash(inode);
  mark_inode_dirty(inode);

  return(uzix__link(dentry, inode));
}

// finished:   yes
// endianness: n/a
static int uzix_create(lnx_inode * dir, lnx_dentry *dentry, int mode)
{
#ifdef UZIX_DEBUG
  printk("uzix: uzix_create\n");
#endif
  return uzix_mknod(dir, dentry, mode, 0);
}


// finished:   yes
// endianness: n/a
static int uzix_mkdir(lnx_inode * dir, lnx_dentry *dentry, int mode) 
{
  lnx_sblock *sb = dir->i_sb;
  lnx_inode *inode;
  int i;
  uz_dentry dpage[UZIX_DPB];

#ifdef UZIX_DEBUG
  printk("uzix: uzix_mkdir\n");
#endif

  inode = new_inode(sb);
  if (!inode) return -ENOMEM;

  i = uzix_alloc_inode(sb);
  if (i<0) { iput(inode); return i; }
  
  /* increment ref count on parent */
  dir->i_nlink++;
  mark_inode_dirty(dir);

  /* build inode */
  inode->i_ino    = i;
  inode->i_mode   = S_IFDIR | mode;
  if (dir->i_mode & S_ISGID) inode->i_mode |= S_ISGID;
  inode->i_uid    = current->fsuid;
  inode->i_gid    = (dir->i_mode & S_ISGID) ? dir->i_gid : current->fsgid;
  inode->i_mtime  = inode->i_atime = inode->i_ctime = CURRENT_TIME;
  inode->i_blocks = inode->i_blksize = 0;
  inode->i_size   = 0;
  inode->i_nlink  = 2; /* parent and . */

  if (uzix_inode_grow(inode, 32)!=0) return -EIO;

  memset(dpage, 0, 512);
  
  dpage[0].d_name[0] = '.';
  dpage[0].d_ino     = (__u16) (inode->i_ino);

  dpage[1].d_name[0] = '.';
  dpage[1].d_name[1] = '.';
  dpage[1].d_ino     = (__u16) (dir->i_ino);

  /* write out dir contents */
  i = UZIX_DBLOCK(inode,0);
  if (uzix_write_block(sb, i, (void *) dpage)!=0)
    return -EIO;

  uzix_set_inode(inode);
  insert_inode_hash(inode);
  mark_inode_dirty(inode);
  uzix_write_inode(inode,0); /* ??? why do I need this here and not
                                in the mknod operation ? odd. */
  return(uzix__link(dentry, inode));
}

// finished:   yes
// endianness: n/a
static int uzix_rmdir(lnx_inode * dir, lnx_dentry *dentry) {
  lnx_inode *inode = dentry->d_inode;
  int nd,err;

#ifdef UZIX_DEBUG
  printk("uzix: uzix_rmdir\n");
#endif

  nd = uzix__count(inode);
  if (nd > 2) return -ENOTEMPTY;

  err = uzix_unlink(dir, dentry);
  dir->i_nlink--;
  inode->i_nlink--;
  mark_inode_dirty(dir);
  mark_inode_dirty(inode);
  return err;
}

// adds an entry to dentry's d_parent dir referencing the given
// inode. this is NOT uzix_link (perform hard link)
// 
// finished:   yes
// endianness: yes
static int uzix__link(lnx_dentry *dentry, lnx_inode *inode) {
  lnx_inode *dir    = dentry->d_parent->d_inode;
  const char *name  = dentry->d_name.name;
  int namelen       = dentry->d_name.len;
  lnx_sblock *sb    = dir->i_sb;
  uz_dentry de;
  int nd;
  unsigned pos, nsz;
  int err=0, rank, offset, blk;
  uz_dentry dblk[UZIX_DPB];

#ifdef UZIX_DEBUG
  printk("uzix: uzix__link\n");
#endif

  de.d_ino = cpu_to_le16((__u16) (inode->i_ino));
  if (namelen > UZIX_MAXNAME) namelen = UZIX_MAXNAME;
  memset(de.d_name, 0, 14);
  memcpy(de.d_name, name, namelen);

  nd = uzix__count(dir);
  pos = nd * 16;
  nsz = pos + 16;

  /* directory inode must grow */
  if (nsz > dir->i_size) {
    err=uzix_inode_grow(dir, nsz);
    if (err!=0) return err;
  }

  rank   = pos / UZIX_BLOCK_SIZE;
  offset = pos % UZIX_BLOCK_SIZE;
  offset = offset / 16;
  
  blk = uzix_block_by_rank(dir, rank);
  if (blk<0) return -EIO;

  if (uzix_read_block(sb, blk, (void *) dblk)!=0)
    return -EIO;

  memcpy(&dblk[offset],&de,sizeof(uz_dentry));

  if (uzix_write_block(sb, blk, (void *) dblk)!=0)
    return -EIO;
  
  d_instantiate(dentry, inode);
  return 0;
}

/* counts the actual number of entries in a dir. UZIX has a bad
   habit of keeping i_size wrong (larger than the real size) */
static int uzix__count(lnx_inode *inode) {
  int i,j,k,x,nd=0;
  uz_dentry buf[UZIX_DPB];

#ifdef UZIX_DEBUG
  printk("uzix: uzix__count\n");
#endif

  j = uzix_fit_bytes(inode->i_size);

  for(i=0;i<j;i++) {
    k = uzix_block_by_rank(inode, i);
    if (k<0) goto out_now;
    if (uzix_read_block(inode->i_sb, k, (void *) buf)!=0)
      goto out_now;

    for(x=0;x<UZIX_DPB;x++) {
      if (buf[x].d_ino!=0 && buf[x].d_name[0]!=0)
	++nd;
      else
	goto out_now;
    }
  }

 out_now:
  return nd;
}

static int uzix_sync_file(struct file * file, lnx_dentry *dentry, int datasync)
{
  #ifdef UZIX_DEBUG
    printk("uzix: uzix_sync_file\n");
  #endif
  return 0;
}

static int uzix_page_rw(struct page * page, int writing, int unlock) {
  lnx_inode *inode = page->mapping->host;
  unsigned long offset, avail, readlen, crun, crem;
  void *buf;
  int rank, boff, blk;
  int result = -ENOSPC;
  __u8 dblk[UZIX_BLOCK_SIZE];

#ifdef UZIX_DEBUG
  printk("uzix: uzix_page_rw\n");
#endif
  
  lock_kernel();
  buf = kmap(page);
  if (!buf)
    goto out_err;

  offset = page->index << PAGE_CACHE_SHIFT;
  if (offset < inode->i_size) {
    avail   = inode->i_size - offset;
    readlen = min_t(unsigned long, avail, PAGE_SIZE);

    crun = 0;
    while(crun < readlen) {
      rank = offset / UZIX_BLOCK_SIZE;
      boff = offset % UZIX_BLOCK_SIZE;
      blk = uzix_block_by_rank(inode, rank);
      if (blk < 0) 
	goto out_err;

      crem = readlen - crun;
      if (crem > UZIX_BLOCK_SIZE - boff)
	crem = UZIX_BLOCK_SIZE - boff;

      if (uzix_read_block(inode->i_sb, blk, dblk)!=0)
	goto out_err;

      if (writing) {
	memcpy(dblk+boff, buf+crun, crem);
	if (uzix_write_block(inode->i_sb, blk, dblk)!=0)
	  goto out_err;
      } else {
	memcpy(buf+crun, dblk+boff, crem);
      }

      offset += crem;
      crun   += crem;
    }
    if (readlen < PAGE_SIZE && !writing)
      memset(buf+readlen,0,PAGE_SIZE-readlen);
    SetPageUptodate(page);
    result = 0;
  }
  if (result) {
    if (!writing) memset(buf,0,PAGE_SIZE);
    SetPageError(page);
  }

 out_err:

  if (buf) kunmap(page);

  /* when called from commit_write we don't want to 
     unlock the page */
  if (unlock)
    UnlockPage(page);

  //  page_cache_release(page);
  unlock_kernel();
  return result;
}

static int uzix_readpage(struct file *file, struct page * page) {
#ifdef UZIX_DEBUG
  printk("uzix: uzix_readpage\n");
#endif  
  return(uzix_page_rw(page,0,1));
}

static int uzix_writepage(struct page * page) {
#ifdef UZIX_DEBUG
  printk("uzix: uzix_writepage\n");
#endif
  return(uzix_page_rw(page,1,1));
}

static int uzix_prepare_write(struct file *file, struct page *page,
			      unsigned offset, unsigned to)
{
  lnx_inode *inode  = page->mapping->host;
  loff_t pos = ((loff_t)page->index << PAGE_CACHE_SHIFT) + to;
  void *addr;
  int  err;

#ifdef UZIX_DEBUG
  printk("uzix: uzix_prepare_write <off:%d,len:%d>\n",(int)offset,(int)to);
#endif

  addr = kmap(page);

  if (pos > inode->i_size) {
    err = uzix_inode_grow(inode, pos);
    if (err != 0) {
      kunmap(page);
      ClearPageUptodate(page);
      return err;
    }
  }

  if (!Page_Uptodate(page)) {
    memset(addr, 0 , PAGE_CACHE_SIZE);
    flush_dcache_page(page);
    SetPageUptodate(page);
  }

  SetPageDirty(page);
  return 0;
}

static int uzix_commit_write(struct file *file, struct page *page, 
			     unsigned offset, unsigned to)
{
  lnx_inode *inode  = page->mapping->host;
  loff_t pos = ((loff_t)page->index << PAGE_CACHE_SHIFT) + to;

#ifdef UZIX_DEBUG
  printk("uzix: uzix_commit_write <off:%d,len:%d>\n",(int)offset,(int)to);
#endif

  kunmap(page);

  /* prepare_write must make the inode grow first */
  if (pos > inode->i_size) {
    ClearPageUptodate(page);
    printk("UZIX-fs: this shouldn't ever happen.\n");
    return -ENOSPC;
  }

  return(uzix_page_rw(page,1,0));
}

static int uzix_inode_grow(lnx_inode *inode, unsigned newlen) {
  lnx_sblock *sb = inode->i_sb;
  int err = 0;
  int oldc, newc, cbc, i, blk, x, y, z;
  __u16 index[256];

#ifdef UZIX_DEBUG
  printk("uzix: uzix_inode_grow <ino:%d,len:%d>\n",(int)(inode->i_ino),
	 (int)newlen);
#endif

  oldc = uzix_fit_bytes(inode->i_size);
  newc = uzix_fit_bytes(newlen);

#ifdef UZIX_DEBUG
  printk("uzix: uzix_inode_grow blocks %d -> %d\n",oldc,newc);
#endif

  if (oldc >= newc) {
    inode->i_size = newlen;
    mark_inode_dirty(inode);
    return 0;
  }
  cbc = oldc;

  /* allocate new blocks */
  for(i=oldc;i<newc;i++) {
    switch(i) {
    case 0 ... 17:        /* direct blocks */
      blk = uzix_alloc_block(sb);
      if (blk<0) { err = -ENOSPC; goto out_err; }
      UZIX_DBLOCK(inode,i) = (__u16) blk;
      ++cbc;
      break;

    case 18:              /* single indirect, no index */
      blk = uzix_alloc_block(sb);
      if (blk<0) { err = -ENOSPC; goto out_err; }
      UZIX_SIBLOCK(inode) = (__u16) blk;
      memset(index,0, 256);
      blk = uzix_alloc_block(sb);
      if (blk<0) { err = -ENOSPC; goto out_err; }
      index[0] = cpu_to_le16((__u16)blk);
      uzix_write_block(sb, UZIX_SIBLOCK(inode), (void *) index);
      ++cbc;
      break;

    case 19 ... 273:      /* single indirect, index exists */
      if (uzix_read_block(sb,UZIX_SIBLOCK(inode),(void *)index)!=0) {
	err = -EIO;
	goto out_err;
      }
      blk = uzix_alloc_block(sb);
      if (blk<0) { err = -ENOSPC; goto out_err; }
      index[i-18] = cpu_to_le16((__u16)blk);
      uzix_write_block(sb, UZIX_SIBLOCK(inode), (void *) index);
      ++cbc;
      break;

    default:              /* double indirect */
      // load first index
      if (uzix_read_block(sb,UZIX_DIBLOCK(inode),(void *)index)!=0) {
	err = -EIO;
	goto out_err;
      }

      x = (i-274) / 256;
      y = (i-274) % 256;

      // alloc 2nd-level index if it doesn't exist
      if (y==0) {
	blk = uzix_alloc_block(sb);
	if (blk<0) { err = -ENOSPC; goto out_err; }
	index[x] = cpu_to_le16( (__u16) blk );
	uzix_write_block(sb, UZIX_DIBLOCK(inode), (void *) index);
      }

      // load 2nd-level index
      z = le16_to_cpu(index[x]);

      if (uzix_read_block(sb,z,(void *)index)!=0) {
	err = -EIO;
	goto out_err;
      }

      // allocate the actual block
      blk = uzix_alloc_block(sb);
      if (blk<0) { err = -ENOSPC; goto out_err; }
      index[y] = cpu_to_le16( (__u16) blk );

      // write 2nd-level index
      uzix_write_block(sb,z,(void *)index);
      ++cbc;

      break;

    } /* switch i */
  } /* for i */

  inode->i_size = newlen;
  mark_inode_dirty(inode);
  return 0;
  
 out_err:
  if (cbc * UZIX_BLOCK_SIZE > newlen)
    inode->i_size = newlen;
  else
    inode->i_size = cbc * UZIX_BLOCK_SIZE;
  mark_inode_dirty(inode);
  return err;
}

static lnx_dentry *uzix_lookup(lnx_inode * dir, lnx_dentry *dentry) {
  lnx_inode * inode = NULL;
  ino_t ino;

#ifdef UZIX_DEBUG
  printk("uzix: uzix_lookup\n");
#endif

  dentry->d_op = dir->i_sb->s_root->d_op;
  
  if (dentry->d_name.len > UZIX_MAXNAME)
    return ERR_PTR(-ENAMETOOLONG);
  
  ino = uzix_inode_of(dentry);
  if (ino) {
    inode = iget(dir->i_sb, ino);
    
    if (!inode)
      return ERR_PTR(-EACCES);
  }
  d_add(dentry, inode);
  return NULL;
}

static ino_t uzix_inode_of(lnx_dentry *dentry) {

  const char *  name    = dentry->d_name.name;
  lnx_inode *   dir     = dentry->d_parent->d_inode;
  lnx_sblock *  sb      = dir->i_sb;
  ino_t         res     = 0;
  int           blk;
  __u16         i, j, n, maxb, maxn;

  uz_dentry buf[UZIX_DPB];

  maxb = uzix_fit_bytes(dir->i_size);
  maxn = dir->i_size / sizeof(uz_dentry);

  for(n=0,i=0;i<maxb;i++) {
    blk = uzix_block_by_rank(dir, i);
    if (blk < 0) goto out_err;
    if (uzix_read_block(sb, blk, (void *) buf)!=0) goto out_err;

    for(j=0;j<UZIX_DPB;j++,n++) {
      if (n >= maxn) goto out_err;
      if (strncmp(name, buf[j].d_name,UZIX_MAXNAME)==0) {
	res = le16_to_cpu(buf[j].d_ino);
	return res;
      }      
    }
  }

 out_err:
  return 0;
}

// makes a hard link
// finished:    yes
// endianness:  n/a
static int          uzix_link(lnx_dentry * old_dentry, lnx_inode * dir,
			      lnx_dentry *dentry)
{
  lnx_inode *inode = old_dentry->d_inode;

#ifdef UZIX_DEBUG
  printk("uzix: uzix_link\n");
#endif

  if (S_ISDIR(inode->i_mode))
    return -EPERM;
  /* UZIX fs supports up to 64K links, but why would you want
     30000 links to the same file ? you're nuts and we're not
     ok with that */
  if (inode->i_nlink > 30000)
    return -EMLINK;

  inode->i_ctime = CURRENT_TIME;
  inode->i_nlink++;
  atomic_inc(&(inode->i_count));
  mark_inode_dirty(inode);
  return(uzix__link(dentry, inode));
}

// finished:    yes
// endianness:  n/a
static int uzix_unlink(lnx_inode * dir, lnx_dentry *dentry) {
  int err = 0;
  lnx_inode * inode = dentry->d_inode;
  int ino;

#ifdef UZIX_DEBUG
  printk("uzix: uzix_unlink\n");
#endif

  ino = uzix_inode_of(dentry);
  if (ino==0) 
    return -ENOENT;

  /* remove entry from the directory */
  err = uzix__rmentry(dir, dentry);

  /* update inode */
  inode->i_ctime = dir->i_ctime;
  inode->i_nlink--;
  mark_inode_dirty(inode);

  return err;
}

static int uzix__rmentry(lnx_inode *dir, lnx_dentry *dentry) {
  int i,j,k,m,n,o,nd,nblocks,err=0,x=-1;
  lnx_sblock *sb   = dir->i_sb;
  const char *name = dentry->d_name.name;
  int namelen      = dentry->d_name.len;
  uz_dentry dblk[UZIX_DPB], xblk[UZIX_DPB];

#ifdef UZIX_DEBUG
  printk("uzix: uzix__rmentry\n");
#endif  

  nd = uzix__count(dir);
  if (namelen > UZIX_MAXNAME) namelen = UZIX_MAXNAME;
  nblocks = uzix_fit_bytes(nd * 16);

  for(i=0;i<nblocks;i++) {
    j = uzix_block_by_rank(dir, i);
    if (j<0) return -EIO;
    if (uzix_read_block(sb, j, (void *) dblk)!=0)
      return -EIO;    
    for(k=0;k<UZIX_DPB;k++) {
      if (strncmp(name, dblk[k].d_name, namelen)==0) {
	x = (i*UZIX_DPB) + k;
	goto found;
      }
    }
  } /* for i */
  if (x<0) return -ENOENT;
 found: /* dblk contains the found entry already */
  i = (nd-1) / UZIX_DPB;
  j = (nd-1) % UZIX_DPB;
  k = uzix_block_by_rank(dir, i);
  if (k<0) return -EIO;

  m = x / UZIX_DPB;
  n = x % UZIX_DPB;
  o = uzix_block_by_rank(dir, m);
  if (o<0) return -EIO;

  if (k != o) {

    /* found entry is not in the last block */
    if (uzix_read_block(sb, k, (void *) xblk)!=0)
      return -EIO;

    /* copy last entry to the deleted position */
    memcpy(&dblk[n],&xblk[j],sizeof(uz_dentry));
    err = uzix_write_block(sb, o, (void *) dblk);

    /* zero out deleted position */
    memset(&xblk[j],0,sizeof(uz_dentry));
    err |= uzix_write_block(sb, k, (void *) xblk);    

  } else {

    /* found entry is in the last block */

    /* copy last entry to the deleted position */
    memcpy(&dblk[n],&dblk[j],sizeof(uz_dentry));
    /* zero out last entry */
    memset(&dblk[j],0,sizeof(uz_dentry));
    err = uzix_write_block(sb, o, (void *) dblk);

  }
  if (err!=0) err = -EIO;

  i = uzix_fit_bytes(nd*16);
  j = uzix_fit_bytes((nd-1)*16);

  /* annoying part, must deallocate one block from the inode */
  if (i!=j) {
    if (i-j > 1)
      printk("UZIX-fs: this is impossible, your computer is clearly nuts.\n");
    uzix__UGLY_free_last_block(dir);
  }

  dir->i_size = (nd-1) * 16;
  mark_inode_dirty(dir);
  return err;
}

/* it DOES NOT fix inode->i_size. If you don't know what you're
   doing, don't call this function -- FB */
static int uzix__UGLY_free_last_block(lnx_inode *inode) {
  lnx_sblock *sb = inode->i_sb;
  int i,j,k;
  int blk,bl2,bl3,off,nblocks;
  __u16 buf[256];

#ifdef UZIX_DEBUG
  printk("uzix: free last block of inode %d\n",(int)(inode->i_ino));
#endif

  nblocks = uzix_fit_bytes(inode->i_size);
  i = nblocks - 1;

  switch(nblocks) {
  case 0 ... 18:

    blk = UZIX_DBLOCK(inode, i);
    UZIX_DBLOCK(inode, i) = 0;
    mark_inode_dirty(inode);
    if (uzix_free_block(sb,blk)!=0) return -EIO;
    return 0;

  case 19: /* we have ONE single indirect block. BASTARD!!! */
    blk = UZIX_SIBLOCK(inode);
    if (uzix_read_block(sb, blk, (void *) buf)!=0)
      return -EIO;
    bl2 = le16_to_cpu(buf[0]);
    buf[0] = 0;
    if (uzix_free_block(sb,bl2)!=0) return -EIO;
    /* free the index */
    if (uzix_free_block(sb,blk)!=0) return -EIO;
    UZIX_SIBLOCK(inode) = 0;
    mark_inode_dirty(inode);
    return 0;

  case 20 ... 273: /* last block is single indirect, but not the only */
    blk = UZIX_SIBLOCK(inode);
    if (uzix_read_block(sb, blk, (void *) buf)!=0)
      return -EIO;
    off = i-18;
    bl2 = le16_to_cpu(buf[i]);
    buf[i] = 0;
    if (uzix_free_block(sb,bl2)!=0) return -EIO;
    /* write back the index */
    if (uzix_write_block(sb, blk, (void *) buf)!=0)
      return -EIO;
    /* inode isn't touched */
    return 0;

  case 274: /* last block is the only double indirect */
    blk = UZIX_DIBLOCK(inode);
    if (uzix_read_block(sb, blk, (void *) buf)!=0)
      return -EIO;
    bl2 = le16_to_cpu(buf[0]);
    
    if (uzix_read_block(sb, bl2, (void *) buf)!=0)
      return -EIO;

    bl3 = le16_to_cpu(buf[0]);
    if (uzix_free_block(sb, bl3)!=0) return -EIO;
    if (uzix_free_block(sb, bl2)!=0) return -EIO;
    if (uzix_free_block(sb, blk)!=0) return -EIO;
    
    UZIX_DIBLOCK(inode) = 0;
    mark_inode_dirty(inode);
    return 0;

  case 275 ... 65535: /* top levels aren't going away */

    blk = UZIX_DIBLOCK(inode);
    if (uzix_read_block(sb, blk, (void *) buf)!=0)
      return -EIO;

    j = (i-274) / 256;
    k = (i-274) % 256;

    bl2 = le16_to_cpu(buf[j]);

    if (uzix_read_block(sb, bl2, (void *) buf)!=0)
      return -EIO;

    bl3 = le16_to_cpu(buf[k]);

    /* the leaf block goes for sure */
    if (uzix_free_block(sb,bl3)!=0) return -EIO;
    buf[k] = 0;
    if (uzix_write_block(sb, bl2, (void *) buf)!=0)
      return -EIO;

    /* if it was the first on its level2-index... */
    if (k==0) {
      if (uzix_read_block(sb, blk, (void *) buf)!=0)
	return -EIO;
      if (uzix_free_block(sb,bl2)!=0) return -EIO;
      buf[j] = 0;
      if (uzix_write_block(sb, blk, (void *) buf)!=0)
	return -EIO;
      // the case j==0 && k==0 is i=274, treated above
    }
    
    // again, we don't even touch the inode proper
    return 0;
  default:
    printk("UZIX-fs: DUCK!!!\n");
    return 0;
  }
}

static int uzix_rename(lnx_inode * old_dir, lnx_dentry * old_dentry,
		       lnx_inode * new_dir, lnx_dentry * new_dentry) 
{
  lnx_inode * old_inode = old_dentry->d_inode;
  lnx_inode * new_inode = new_dentry->d_inode;
  int err;

#ifdef UZIX_DEBUG
  printk("uzix: uzix_rename\n");
#endif

  /* remove entry from old_dir */
  err = uzix__rmentry(old_dir, old_dentry);
  if (err!=0) return err;

  if (new_inode) {
    printk("UZIX-fs: FIXME - what did you do to generate this message ? email bergo@seul.org about it.\n");
    return -EPERM;
  } else {
    err = uzix__link(new_dentry, old_inode);
  }
  return 0;
}

static int uzix_symlink(lnx_inode * dir, lnx_dentry *dentry,
			const char * symname)
{
  lnx_sblock *sb = dir->i_sb;
  lnx_inode *inode;
  int i,j,k;
  __u8 blk[512];

#ifdef UZIX_DEBUG
  printk("uzix: uzix_symlink\n");
#endif

  k = strlen(symname);
  if (k > 512) return -ENAMETOOLONG;

  inode = new_inode(sb);
  if (!inode) return -ENOMEM;

  i = uzix_alloc_inode(sb);
  if (i<0) { iput(inode); return i; }
  j = uzix_alloc_block(sb);
  if (j<0) { iput(inode); return j; }

  inode->i_ino    = i;
  inode->i_uid    = current->fsuid;
  inode->i_gid    = (dir->i_mode & S_ISGID) ? dir->i_gid : current->fsgid;
  inode->i_mtime  = inode->i_atime = inode->i_ctime = CURRENT_TIME;
  inode->i_blocks = inode->i_blksize = 0;
  inode->i_mode   = S_IFLNK | 0777;
  inode->i_size   = k;
  UZIX_DBLOCK(inode,0) = j;
  uzix_set_inode(inode);
  insert_inode_hash(inode);
  mark_inode_dirty(inode);

  memset(blk,0,512);
  memcpy(blk,symname,k);
  if (uzix_write_block(sb,j,(void *) blk)!=0)
    return -EIO;
  return(uzix__link(dentry, inode));
}

/* SECTION: MODULE DECLARATION */

static DECLARE_FSTYPE_DEV(uzix_fs_type,"uzix",uzix_read_super);

static int __init init_uzix_fs(void)
{
        return register_filesystem(&uzix_fs_type);
}

static void __exit exit_uzix_fs(void)
{
        unregister_filesystem(&uzix_fs_type);
}

EXPORT_NO_SYMBOLS;

module_init(init_uzix_fs)
module_exit(exit_uzix_fs)
MODULE_LICENSE("GPL");
