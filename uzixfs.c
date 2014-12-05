
/* 
   Uzix X-Utils (cross platform utilities)   
   (C) 2003 Felipe Bergo - bergo@seul.org 

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License,
   version 2 or (at your option) any later version. The license
   is included in the COPYING file.
*/

#include <time.h>
#include "uzixfs.h"
#include "byteorder.h"

int uz_read_sblock(FILE *f, uz_sblock *sb) {
  
  if (fseek(f,UZ_SBLOCK * UZ_BLOCKSZ,SEEK_SET)!=0) return -1;
  
  if (read_u16(f,&(sb->s_mounted),1)!=0) return -1;
  if (read_u16(f,&(sb->s_reserv),1)!=0) return -1;
  if (read_u16(f,&(sb->s_isize),1)!=0) return -1;
  if (read_u16(f,&(sb->s_fsize),1)!=0) return -1;

  if (read_u16(f,&(sb->s_tfree),1)!=0) return -1;
  if (read_u16(f,&(sb->s_nfree),1)!=0) return -1;
  if (read_u16(f,&(sb->s_free[0]),50)!=0) return -1;

  if (read_u16(f,&(sb->s_tinode),1)!=0) return -1;
  if (read_u16(f,&(sb->s_ninode),1)!=0) return -1;
  if (read_u16(f,&(sb->s_inode[0]),50)!=0) return -1;

  if (read_u16(f,&(sb->s_time.t_time),1)!=0) return -1;
  if (read_u16(f,&(sb->s_time.t_date),1)!=0) return -1;

  return 0;
}

int uz_write_sblock(FILE *f, uz_sblock *sb) {
  if (fseek(f,UZ_SBLOCK * UZ_BLOCKSZ,SEEK_SET)!=0) return -1;
  
  if (write_u16(f,&(sb->s_mounted),1)!=0) return -1;
  if (write_u16(f,&(sb->s_reserv),1)!=0) return -1;
  if (write_u16(f,&(sb->s_isize),1)!=0) return -1;
  if (write_u16(f,&(sb->s_fsize),1)!=0) return -1;

  if (write_u16(f,&(sb->s_tfree),1)!=0) return -1;
  if (write_u16(f,&(sb->s_nfree),1)!=0) return -1;
  if (write_u16(f,&(sb->s_free[0]),50)!=0) return -1;

  if (write_u16(f,&(sb->s_tinode),1)!=0) return -1;
  if (write_u16(f,&(sb->s_ninode),1)!=0) return -1;
  if (write_u16(f,&(sb->s_inode[0]),50)!=0) return -1;

  if (write_u16(f,&(sb->s_time.t_time),1)!=0) return -1;
  if (write_u16(f,&(sb->s_time.t_date),1)!=0) return -1;

  return 0;
}

int uz_read_inode(FILE *f, uz_sblock *sb, uz_ino_t no, uz_inode *inode) {

  if (fseek(f,(UZ_BLOCKSZ * sb->s_reserv) + (UZ_ILEN * no), SEEK_SET)!=0)
    return -1;

  if (read_u16(f,&(inode->i_mode),1)!=0) return -1;
  if (read_u16(f,&(inode->i_nlink),1)!=0) return -1;
  if (read_u8(f,&(inode->i_uid),1)!=0) return -1;
  if (read_u8(f,&(inode->i_gid),1)!=0) return -1;
  if (read_s32(f,&(inode->i_size),1)!=0) return -1;
  if (read_u16(f,&(inode->i_atime.t_time),1)!=0) return -1;
  if (read_u16(f,&(inode->i_atime.t_date),1)!=0) return -1;
  if (read_u16(f,&(inode->i_mtime.t_time),1)!=0) return -1;
  if (read_u16(f,&(inode->i_mtime.t_date),1)!=0) return -1;
  if (read_u16(f,&(inode->i_ctime.t_time),1)!=0) return -1;
  if (read_u16(f,&(inode->i_ctime.t_date),1)!=0) return -1;
  if (read_u16(f,&(inode->i_addr[0]),20)!=0) return -1;
  if (read_u16(f,&(inode->i_dummy),1)!=0) return -1;

  return 0;
}

int uz_write_inode(FILE *f, uz_sblock *sb, uz_ino_t no, uz_inode *inode) {

  if (fseek(f,(UZ_BLOCKSZ * sb->s_reserv) + (UZ_ILEN * no), SEEK_SET)!=0)
    return -1;

  if (write_u16(f,&(inode->i_mode),1)!=0) return -1;
  if (write_u16(f,&(inode->i_nlink),1)!=0) return -1;
  if (write_u8(f,&(inode->i_uid),1)!=0) return -1;
  if (write_u8(f,&(inode->i_gid),1)!=0) return -1;
  if (write_s32(f,&(inode->i_size),1)!=0) return -1;
  if (write_u16(f,&(inode->i_atime.t_time),1)!=0) return -1;
  if (write_u16(f,&(inode->i_atime.t_date),1)!=0) return -1;
  if (write_u16(f,&(inode->i_mtime.t_time),1)!=0) return -1;
  if (write_u16(f,&(inode->i_mtime.t_date),1)!=0) return -1;
  if (write_u16(f,&(inode->i_ctime.t_time),1)!=0) return -1;
  if (write_u16(f,&(inode->i_ctime.t_date),1)!=0) return -1;
  if (write_u16(f,&(inode->i_addr[0]),20)!=0) return -1;
  if (write_u16(f,&(inode->i_dummy),1)!=0) return -1;

  return 0;
}

int uz_read_data(FILE *f, uz_inode *inode, 
		 uint32_t offset, uint32_t length, void *dest)
{
  int nth, block, boff, maxpayload, payload, accpayload = 0;
  char local[UZ_BLOCKSZ];

  if (offset >= inode->i_size)
    return 0;

  if (offset + length > inode->i_size)
    length = inode->i_size - offset;

  while(length != 0) {
    nth = offset / UZ_BLOCKSZ;
    block = uz_xlate_block(f, inode, nth);
    if (block < 0) return -1;
    boff = offset % UZ_BLOCKSZ;
    
    if (uz_read_raw_block(f,block,local)!=0)
      return -1;
  
    maxpayload = UZ_BLOCKSZ - boff;
    if (length < maxpayload)
      payload = length;
    else
      payload = maxpayload;

    memcpy(dest,local+boff,payload);
    offset += payload;
    length -= payload;    
    dest = dest + payload;
    accpayload += payload;
  }

  return accpayload;
}

int uz_write_data(FILE *f, uz_inode *inode, 
		  uint32_t offset, uint32_t length, void *src)
{
  int nth, block, boff, maxpayload, payload, accpayload = 0;
  char local[UZ_BLOCKSZ];

  if (offset >= inode->i_size)
    return 0;

  if (offset + length > inode->i_size)
    length = inode->i_size - offset;

  while(length != 0) {
    nth = offset / UZ_BLOCKSZ;
    block = uz_xlate_block(f, inode, nth);
    if (block < 0) return -1;
    boff = offset % UZ_BLOCKSZ;
    
    if (uz_read_raw_block(f,block,local)!=0)
      return -1;
  
    maxpayload = UZ_BLOCKSZ - boff;
    if (length < maxpayload)
      payload = length;
    else
      payload = maxpayload;

    memcpy(local+boff,src,payload);
    if (uz_write_raw_block(f,block,local)!=0)
      return -1;

    offset += payload;
    length -= payload;    
    src = src + payload;
    accpayload += payload;
  }

  return accpayload;
}

/* 18 direct blocks                  :   0 ..    17
   256 indirect blocks               :  18 ..   273
   256 * 256 double indirect blocks  : 274 .. 65809  */

/* translates "the rank-th data block of this inode" into a physical block number */

int uz_xlate_block(FILE *f, uz_inode *inode, int rank) {
  uz_blkno_t local[256];
  int x,y;

  // direct
  if (rank < 18)
    return(inode->i_addr[rank]);

  // single indirect
  if (rank >= 18 && rank < 274) {
    if (uz_read_raw_block(f,inode->i_addr[18],(void *) local)!=0) return -1;
    rank -= 18;
    x = u16_to_le(local[rank]);
    return x;
  }

  // double indirect
  if (rank >= 274 && rank <= 65809) {
    if (uz_read_raw_block(f,inode->i_addr[19],(void *) local)!=0) return -1;
    rank -= 274;
    y = rank / 256;
    local[y] = u16_to_le(local[y]);
    if (uz_read_raw_block(f,local[y],(void *) local)!=0) return -1;
    y = rank % 256;
    x = u16_to_le(local[y]);
    return x;    
  }

  return -1;
}

int uz_set_nth_block(FILE *f, uz_inode *inode, int rank, uz_blkno_t block) {
  uz_blkno_t local[256];
  int y,x;

  // direct
  if (rank < 18) {
    inode->i_addr[rank] = block;
    return 0;
  }

  // single indirect
  if (rank >= 18 && rank < 274) {
    if (uz_read_raw_block(f,inode->i_addr[18],(void *) local)!=0) return -1;
    rank -= 18;
    block = u16_to_le(block);
    local[rank] = block;
    if (uz_write_raw_block(f,inode->i_addr[18],(void *) local)!=0) return -1;
    return 0;
  }

  // double indirect
  if (rank >= 274 && rank <= 65809) {
    if (uz_read_raw_block(f,inode->i_addr[19],(void *) local)!=0) return -1;
    rank -= 274;
    y = rank / 256;
    x = u16_to_le(local[y]);
    if (uz_read_raw_block(f,x,(void *) local)!=0) return -1;
    y = rank % 256;
    local[y] = u16_to_le(block);
    if (uz_write_raw_block(f,x,(void *) local)!=0) return -1;
    return 0;
  }

  return -1;
}

int uz_read_raw_block(FILE *f, uz_blkno_t block, void *dest) {
  uint32_t offset;
  offset = block;
  offset *= UZ_BLOCKSZ;
  if (fseek(f,offset,SEEK_SET)!=0) return -1;
  if (fread(dest,sizeof(uint8_t),UZ_BLOCKSZ,f)!=UZ_BLOCKSZ) return -1;
  return 0;
}

int uz_write_raw_block(FILE *f, uz_blkno_t block, void *src) {
  uint32_t offset;
  offset = block;
  offset *= UZ_BLOCKSZ;
  if (fseek(f,offset,SEEK_SET)!=0) return -1;
  if (fwrite(src,sizeof(uint8_t),UZ_BLOCKSZ,f)!=UZ_BLOCKSZ) return -1;
  return 0;  
}

uz_blkno_t uz_fit_bytes(uz_off_t length) {
  uz_off_t x;
  x = length / UZ_BLOCKSZ;
  if (length % UZ_BLOCKSZ) ++x;
  return( (uz_blkno_t) x );
}

int uz_inode_remove(FILE *f, uz_sblock *sb, uz_ino_t inode) {
  if (uz_inode_implode(f,sb,inode)!=0) return -1;
  if (uz_free_inode(f,sb,inode)!=0) return -1;
  if (uz_write_sblock(f,sb)!=0) return -1;
  return 0;
}

int uz_inode_implode(FILE *f, uz_sblock *sb, uz_ino_t inode) {
  uz_blkno_t x;
  int i,j,k;
  uz_inode z;
  uint16_t blk[256], iblk[256];

  if (uz_read_inode(f,sb,inode,&z)!=0) return -1;
  x = uz_fit_bytes(z.i_size);

  // direct blocks
  for(i=0;i<x && i<18;i++) {
    if (uz_free_block(f,sb,z.i_addr[i])!=0) return -1;
    z.i_addr[i] = 0;
  }

  // single indirect
  if (x>=18) {
    if (uz_read_raw_block(f,z.i_addr[18],(void *)blk)!=0) return -1;
    for(i=0;i<256;i++) blk[i] = u16_to_le(blk[i]);
    for(i=18;i<274 && i<x;i++)
      if (uz_free_block(f,sb,blk[i-18])!=0) return -1;
    if (uz_free_block(f,sb,z.i_addr[18])!=0) return -1;
    z.i_addr[18] = 0;
  }

  // double indirect
  if (x>=274) {
    if (uz_read_raw_block(f,z.i_addr[19],(void *)iblk)!=0) return -1;
    for(i=0;i<256;i++) iblk[i] = u16_to_le(iblk[i]);
    for(i=274;i<x;i++) {
      j = uz_xlate_block(f,&z,i); if (j<0) return -1;
      if (uz_free_block(f,sb,j)!=0) return -1;
      k = (j-274) / 256;
      if (iblk[k]!=0) {
	if (uz_free_block(f,sb,iblk[k])!=0) return -1;
	iblk[k] = 0;
      }
    }
    if (uz_free_block(f,sb,z.i_addr[19])!=0) return -1;
    z.i_addr[19] = 0;
  }

  z.i_size = 0;
  if (uz_write_inode(f,sb,inode,&z)!=0) return -1;
  if (uz_write_sblock(f,sb)!=0) return -1;
  return 0;
}

int uz_inode_grow(FILE *f, uz_sblock *sb, uz_ino_t inode, uz_off_t length) 
{
  uz_inode x;
  int i, j, k, nblocks, oblocks;
  uint8_t have_sind, have_dindi, have_dind[256];
  uint8_t want_sind, want_dindi, want_dind[256];
  uint16_t blk[256], iblk[256];

  nblocks = uz_fit_bytes(length);
  if (nblocks > 65810) return -1;

  if (uz_read_inode(f,sb,inode,&x)!=0) return -1;

  oblocks = uz_fit_bytes(x.i_size);

  /* no need to remove or add blocks */
  if (nblocks == oblocks) return 0;
  if (nblocks < oblocks) return -1;

  have_sind  = 0;
  have_dindi = 0;
  for(i=0;i<256;i++) have_dind[256] = 0;
  want_sind  = 0;
  want_dindi = 0;
  for(i=0;i<256;i++) want_dind[256] = 0;

  /* calc old block layout */
  if (oblocks >= 18)  have_sind  = 1;
  if (oblocks >= 274) have_dindi = 1;  
  j = (oblocks - 274); /* number of groups of 256-blocks in the last chain */
  k = j/256;
  if (j%256) ++k;
  for(i=0;i<k;i++) have_dind[i] = 1;

  /* calc new block layout */
  if (nblocks >= 18)  want_sind  = 1;
  if (nblocks >= 274) want_dindi = 1;  
  j = (nblocks - 274); /* number of groups of 256-blocks in the last chain */
  k = j/256;
  if (j%256) ++k;
  for(i=0;i<k;i++) want_dind[i] = 1;
  
  // direct blocks
  for(i=oblocks;i<18;i++) {
    j = uz_alloc_block(f,sb); if (j<0) return -1;
    x.i_addr[i] = j;
  }
  
  // single indirect
  if (!have_sind && want_sind) {
    j = uz_alloc_block(f,sb); if (j<0) return -1;
    x.i_addr[18] = j;
  }
  if (want_sind && oblocks<274) {
    if (uz_read_raw_block(f,x.i_addr[18],(void *)blk)!=0) return -1;
    for(i=oblocks;i<nblocks && i<274;i++) {
      j = uz_alloc_block(f,sb); if (j<0) return -1;
      blk[i-18] = u16_to_le(j);
    }
    if (uz_write_raw_block(f,x.i_addr[18],(void *)blk)!=0) return -1;
  }
  
  // double indirect
  if (!have_dindi && want_dindi) {
    j = uz_alloc_block(f,sb); if (j<0) return -1;
    x.i_addr[19] = j;
  }
  if (want_dindi) {
    if (uz_read_raw_block(f,x.i_addr[19],(void *)iblk)!=0) return -1;
    for(i=0;i<256;i++) iblk[i] = u16_to_le(iblk[i]);
    
    for(i=0;i<256;i++)
      if (!have_dind[i] && want_dind[i]) {
	j=uz_alloc_block(f,sb); if (j<0) return -1;
	iblk[i] = j;
      }
    
    /* write back index */
    for(i=0;i<256;i++) iblk[i] = u16_to_le(iblk[i]);
    if (uz_write_raw_block(f,x.i_addr[19],(void *)iblk)!=0) return -1;      
    
    /* now that we have at least ensured that the index blocks exist,
       leave the rest of the work to uz_set_nth_block */
    
    i = oblocks;
    if (i < 274) i = 274;
    for(;i<nblocks;i++) {
      j = uz_alloc_block(f,sb); if (j<0) return -1;
      if (uz_set_nth_block(f,&x,i,j)!=0) return -1;
    }
    
  }

  /* write back inode and superblock */
  x.i_size = length;
  if (uz_write_inode(f,sb,inode,&x)!=0) return -1;
  if (uz_write_sblock(f,sb)!=0) return -1;

  return 0;
}

int uz_alloc_inode(FILE *f, uz_sblock *sb) {
  uz_ino_t newno;
  uz_ino_t i,j;
  uz_inode x;
  int k;

  if (!sb->s_tinode) return -1; /* no inodes left, sorry */

  /* refill inode cache if empty */
  if (sb->s_ninode == 0) {
    j = sb->s_isize * UZ_IPB;
    k = 0;
    /* inode 0 is reserved, 1 is the root directory, they can't possibly
       be free */
    for(i=2;i<j && k<50;i++) {
      if (uz_read_inode(f,sb,i,&x)!=0) return -1;
      if (x.i_mode == 0 && x.i_nlink == 0)
	sb->s_inode[k++] = i;
    }
    sb->s_ninode = k;
  }

  /* get an inode from the cache  */
  if (sb->s_ninode) {
    newno = sb->s_inode[--sb->s_ninode];
    if (newno <= UZ_ROOT || newno >= sb->s_isize * UZ_IPB)
      return -1;
    --sb->s_tinode;
    return newno;
  }

  return -1;
}

int uz_free_inode(FILE *f, uz_sblock *sb, uz_ino_t inode) {
  uz_inode x;

  ++sb->s_tinode;
  if (sb->s_ninode < 50) sb->s_inode[sb->s_ninode++]=inode;

  memset(&x,0,sizeof(uz_inode));
  if (uz_write_inode(f,sb,inode,&x)!=0) return -1;

  return 0;
}

int uz_alloc_block(FILE *f, uz_sblock *sb) {
  uz_blkno_t newno;
  uint16_t nc[256];
  int i;

  /* the cache can never be empty */
  if (sb->s_nfree == 0) return -1; /* filesystem is inconsistent */
  if (sb->s_tfree == 0) return -1; /* filesystem full */

  newno = sb->s_free[--sb->s_nfree];
  if (newno == 0) {
    sb->s_nfree++;
    return -1; /* filesystem is full */
  } 

  --sb->s_tfree; /* account new block */
  
  /* refill cache if needed */
  if (sb->s_nfree == 0) {
    if (uz_read_raw_block(f,newno,(void *)nc)!=0) return -1;
    for(i=0;i<51;i++) nc[i] = u16_to_le(nc[i]);
    sb->s_nfree = nc[0];
    for(i=0;i<50;i++) sb->s_free[i] = nc[i+1];    
  }

  /* wipe out the block */
  memset(nc,0,512);
  if (uz_write_raw_block(f,newno,(void *)nc)!=0) return -1;
  
  return newno;
}

int uz_free_block(FILE *f, uz_sblock *sb, uz_blkno_t block) {
  uint16_t nc[256];
  int i;

  /* flush cache to current block if it's full (the cache) */
  if (sb->s_nfree == 50) {
    memset(nc,0,512);
    nc[0] = sb->s_nfree;
    for(i=0;i<50;i++)
      nc[i+1] = sb->s_free[i];
    for(i=0;i<51;i++) nc[i] = u16_to_le(nc[i]);
    if (uz_write_raw_block(f,block,(void *)nc)!=0) return -1;
    sb->s_nfree = 0;
  }

  ++sb->s_tfree;
  sb->s_free[sb->s_nfree++] = block;

  return 0;
}

char * uz_date_for_humans(uz_time_t *t, char *dest) {
  int h,m,s,D,M,Y;
  static char *mo[13] = { "Jan", "Jan","Feb","Mar","Apr","May","Jun",
			  "Jul","Aug","Sep","Oct","Nov","Dec" };

  h = (t->t_time >> 11) & 0x1f;
  m = (t->t_time >> 5) & 0x3f;
  s = (t->t_time << 1) & 0x3f;

  D = ((t->t_date) & 0x1f);
  M = (t->t_date >> 5) & 0x0f;
  Y = 1980 + ((t->t_date >> 9) & 0x7f);

  if (D==0) ++D;
  
  sprintf(dest,"%s %2d %4d %.2d:%.2d:%.2d",
	  mo[M%13],D,Y,h,m,s);
  return dest;
}

void uz_set_date(int day,int month,int year, uz_time_t *t) {
  month &= 0x0f;
  year  -= 1980;
  year  &= 0x7f;
  t->t_date = (day) | (month << 5) | (year << 9);
}

void uz_set_time(int hour,int min,int sec, uz_time_t *t) {
  hour &=  0x1f;
  min  &=  0x3f;
  sec  >>= 1; 
  sec  &=  0x3f;

  t->t_time = sec | (min <<5) | (hour << 11);
}

void uz_time(uz_time_t *t) {
  time_t t0;
  struct tm *now;

  t0 = time(0);
  now = gmtime(&t0);

  if (now != 0) {
    uz_set_date(now->tm_mday, now->tm_mon + 1, now->tm_year, t);
    uz_set_time(now->tm_hour, now->tm_min, now->tm_sec, t);
  }
}
