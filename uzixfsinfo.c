
/* 
   Uzix X-Utils (cross platform utilities)   
   (C) 2003 Felipe Bergo - bergo@seul.org 

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License,
   version 2 or (at your option) any later version. The license
   is included in the COPYING file.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uzixfs.h"
#include "uzixdir.h"

/* superblock sanity checks */
int consistency_check(uz_sblock *sb) {
  int failures = 0;

  // 1
  if (sb->s_mounted != UZ_SBSIG) {
    printf("!! super block inconsistency: bad signature (%d, expected %d)\n",
	   sb->s_mounted, UZ_SBSIG);
    ++failures;
  }

  // 2
  if (sb->s_reserv <= 1 || sb->s_reserv > sb->s_fsize) {
    printf("!! pointer to inode block is nuts.\n");
    ++failures;
  }


  // 3
  if (sb->s_isize >= sb->s_fsize) {
    printf("!! there are more inode blocks than blocks in the disk.\n");
    ++failures;
  }

  // 4
  if (sb->s_tfree > sb->s_fsize) {
    printf("!! number of free blocks is impossible.\n");
    ++failures;
  }

  // 5
  if (sb->s_nfree == 0 || sb->s_nfree > 50) {
    printf("!! free block list is corrupt.\n");
    ++failures;
  }

  // 6
  if (sb->s_tinode > sb->s_isize * UZ_IPB) {
    printf("!! there are more free inodes than blocks to hold them.\n");
    ++failures;
  }

  // 7
  if (sb->s_ninode > sb->s_tinode) {
    printf("!! free inode cache list is corrupt.\n");
    ++failures;
  }

  printf("7 superblock sanity checks performed, %d failure(s).\n", failures);

  return failures;
}

void showinfo(uz_sblock *sb) {
  int i;
  int u,n,b,kb,total;
  char dbuf[32];
  int ic0 = 0;

  printf("1 block = 512 bytes\n");

  if (consistency_check(sb) > 0)
    return;

  if (sb->s_free[0] == 0) ic0 = 1;

  printf("first inode block         : %d\n",sb->s_reserv);
  printf("inode blocks              : %d\n",sb->s_isize);
  printf("total blocks              : %d\n\n",sb->s_fsize);

  printf("available data blocks     : %d\n",sb->s_tfree);
  printf("cached avail. data blocks : %d\n",ic0 ? sb->s_nfree - 1 : sb->s_nfree);

  if (sb->s_nfree) {
    printf("(block cache: ");
    for(i=ic0?1:0;i<sb->s_nfree;i++)
      printf(" %d",sb->s_free[i]);
    printf(" )\n");
  }

  printf("\n");

  printf("available inodes          : %d\n",sb->s_tinode);
  printf("cached available inodes   : %d\n",sb->s_ninode);

  if (sb->s_ninode) {
    printf("(inode cache: ");
    for(i=0;i<sb->s_ninode;i++)
      printf(" %d",sb->s_inode[i]);
    printf(" )\n");
  }

  printf("\n");

  uz_date_for_humans(&(sb->s_time), dbuf);
  printf("last modified             : %s\n",dbuf);

  printf("\n");

  printf("-------------------+-------------------------------------------------\n");
  printf("usage summary      |      units    blocks     bytes    kbytes       %%\n");
  printf("-------------------+-------------------------------------------------\n");

  n = sb->s_fsize; b = n * UZ_BLOCKSZ; kb = b / 1024;
  total = b;

  printf("total              |  %9s %9d %9d %9d %6.1f%%\n",
	 "-",n,b,kb,(100.0*b)/(float)total);

  n = sb->s_isize; b = n * UZ_BLOCKSZ; kb = b / 1024;
  u = n * UZ_IPB;
  printf("inodes (reserved)  |  %9d %9d %9d %9d %6.1f%%\n",
	 u,n,b,kb,(100.0*b)/(float)total);

  u = (sb->s_isize * UZ_IPB) - sb->s_tinode;
  n = u / UZ_IPB;
  if (u%UZ_IPB) ++n;
  b = n * UZ_BLOCKSZ; kb = b / 1024;
  printf("inodes (used)      |  %9d %9d %9d %9d %6.1f%%\n",
	 u,n,b,kb,(100.0*b)/(float)total);


  n = 2;  b = n * UZ_BLOCKSZ; kb = b / 1024;
  printf("boot+superblock    |  %9s %9d %9d %9d %6.1f%%\n",
	 "-",n,b,kb,(100.0*b)/(float)total);

  n = sb->s_reserv - 2; b = n * UZ_BLOCKSZ; kb = b / 1024;
  printf("reserved/unused    |  %9s %9d %9d %9d %6.1f%%\n",
	 "-",n,b,kb,(100.0*b)/(float)total);

  n = sb->s_fsize - (sb->s_isize + sb->s_reserv); 
  b = n * UZ_BLOCKSZ; kb = b / 1024;
  printf("data blocks (all)  |  %9s %9d %9d %9d %6.1f%%\n",
	 "-",n,b,kb,(100.0*b)/(float)total);

  n = (sb->s_fsize - (sb->s_isize + sb->s_reserv)) - sb->s_tfree; 
  b = n * UZ_BLOCKSZ; kb = b / 1024;
  printf("data blocks (used) |  %9s %9d %9d %9d %6.1f%%\n",
	 "-",n,b,kb,(100.0*b)/(float)total);

  n = sb->s_tfree; 
  b = n * UZ_BLOCKSZ; kb = b / 1024;
  printf("data blocks (free) |  %9s %9d %9d %9d %6.1f%%\n",
	 "-",n,b,kb,(100.0*b)/(float)total);

  printf("-------------------+-------------------------------------------------\n");  
  printf("\n");
}

int main(int argc, char **argv) {

  FILE *f;
  int nf = 0;
  int i;

  uz_sblock sb;

  uz_global_opt(argc, argv);

  for(i=1;i<argc;i++) {
    if (argv[i][0] == '-')
      continue;

    f=fopen(argv[i],"r");
    if (!f) {
      printf("unable to open file %s for reading, skipping.\n",argv[i]);
      continue;
    }
    if (uz_read_sblock(f,&sb)!=0) {
      printf("error reading fs superblock.\n");      
    } else {
      ++nf;
      printf("UZIX fs: %s\n",argv[i]);
      showinfo(&sb);
    }
    fclose(f);
  }

  if (!nf) {
    fprintf(stderr,"usage: uzixfsinfo image.dsk [image2.dsk ...]\n\n");
    return 1;
  }

  return 0;
}
