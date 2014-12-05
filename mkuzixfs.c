
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
#include <stdint.h>
#include "uzixfs.h"
#include "uzixdir.h"
#include "byteorder.h"

char ofile[512];
int  fsize, isize, rsize; /* bytes! */
int  quiet;

/* copied from the MSX-DOS util */
char bootblock[512] = {
  0xEB, 0xFE, 0x90, 'U',  'Z',  'I',  'X',  'd',
  'i',  's',  'k',  0x00, 0x02, 0x02, 0x01, 0x00,
  0x00, 0x00, 0x00, 0xA0, 0x05, 0xF9, 0x00, 0x00,
  0x09, 0x00, 0x02, 0x00, 0x00, 0x00, 0xD0, 0x36,
  0x56, 0x23, 0x36, 0xC0, 0x31, 0x1F, 0xF5, 0x11,
  0x4A, 0xC0, 0x0E, 0x09, 0xCD, 0x7D, 0xF3, 0x0E,
  0x08, 0xCD, 0x7D, 0xF3, 0xFE, 0x1B, 0xCA, 0x22,
  0x40, 0xF3, 0xDB, 0xA8, 0xE6, 0xFC, 0xD3, 0xA8,
  0x3A, 0xFF, 0xFF, 0x2F, 0xE6, 0xFC, 0x32, 0xFF,
  0xFF, 0xC7, 0x57, 0x41, 0x52, 0x4E, 0x49, 0x4E,
  0x47, 0x21, 0x07, 0x0D, 0x0A, 0x0A, 0x54, 0x68,
  0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x61, 0x6E,
  0x20, 0x55, 0x5A, 0x49, 0x58, 0x20, 0x64, 0x69,
  0x73, 0x6B, 0x2C, 0x20, 0x6E, 0x6F, 0x6E, 0x20,
  0x62, 0x6F, 0x6F, 0x74, 0x61, 0x62, 0x6C, 0x65,
  0x2E, 0x0D, 0x0A, 0x55, 0x73, 0x69, 0x6E, 0x67,
  0x20, 0x69, 0x74, 0x20, 0x75, 0x6E, 0x64, 0x65,
  0x72, 0x20, 0x4D, 0x53, 0x58, 0x44, 0x4F, 0x53,
  0x20, 0x63, 0x61, 0x6E, 0x20, 0x64, 0x61, 0x6D,
  0x61, 0x67, 0x65, 0x20, 0x69, 0x74, 0x2E, 0x0D,
  0x0A, 0x0A, 0x48, 0x69, 0x74, 0x20, 0x45, 0x53,
  0x43, 0x20, 0x66, 0x6F, 0x72, 0x20, 0x42, 0x41,
  0x53, 0x49, 0x43, 0x20, 0x6F, 0x72, 0x20, 0x61,
  0x6E, 0x79, 0x20, 0x6B, 0x65, 0x79, 0x20, 0x74,
  0x6F, 0x20, 0x72, 0x65, 0x62, 0x6F, 0x6F, 0x74,
  0x2E, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

void usage() {
  fprintf(stderr,"usage: mkuzixfs [-q] [-o name] [-f size] [-i size] [-r size]\n");
  fprintf(stderr,"size can be specified as a decimal number followed by an optional\n");
  fprintf(stderr,"letter: b,K b=blocks, K=kbytes, nothing=bytes. e.g.: 720K, 25b\n\n");
}

void tick() {
  if (!quiet) {
    printf("#");
    fflush(stdout);
  }
}

int rdsize(char *x) {
  int ll, v;
  char cp[32];
  ll = strlen(x) - 1;
  strcpy(cp, x);
  
  switch(x[ll]) {
  case 'b':
    cp[ll] = 0;
    v = atoi(cp);
    v *= 512;
    break;
  case 'K':
  case 'k':
    cp[ll] = 0;
    v = atoi(cp);
    v *= 1024;
    break;
  default:
    v = atoi(cp);
  }

  if (v%512) {
    fprintf(stderr,"sizes must be multiples of 512 bytes (block size)\n");
    v = -1;
  }
  if (v < 0) v=-1;
  return v;
}

/* mkuzixfs [-q] [-o name] [-f spec] [-i spec] [-r spec] */
int main(int argc, char **argv) {
  int i,j;
  FILE *dsk;
  uint8_t buf[512];
  int fb,ib,rb;
  uz_sblock sb;
  uz_inode  inode;

  uz_direntry rdir[2] = {
    { UZ_ROOT, "." },
    { UZ_ROOT, ".."}
  };

  uz_global_opt(argc, argv);
  
  strcpy(ofile,"newuzix.dsk");
  fsize = 720 * 1024;
  isize = 25  * 512;
  rsize = 0;
  quiet = 0;

  for(i=1;i<argc;i++) {
    if (!strcmp(argv[i],"-q")) {
      quiet = 1;
      continue;
    }
    if (i==argc-1) { usage(); return 1; }   
    if (!strcmp(argv[i],"-o")) { strcpy(ofile,argv[++i]); continue; }
    if (!strcmp(argv[i],"-f")) { fsize = rdsize(argv[++i]); continue; }
    if (!strcmp(argv[i],"-i")) { isize = rdsize(argv[++i]); continue; }
    if (!strcmp(argv[i],"-r")) { rsize = rdsize(argv[++i]); continue; }
  }

  if (fsize < 0 || rsize < 0 || isize < 0) {
    fprintf(stderr,"** illegal f/i/r size specification.\n\n");
    return 2;
  }

  if (rsize+isize+1024 > fsize) {
    fprintf(stderr,"** isize and rsize too large to fsize\n\n");
    return 2;
  }

  if (!quiet)
    printf("mkuzixfs v1.0 - written by Felipe Bergo\n");

  if (!quiet)
    printf("creating filesystem: %d blocks (%d for inodes, %d reserved, %d data)\n",
	   fsize/512, isize/512, rsize/512, (fsize-isize-rsize-1024)/512);

  dsk = fopen(ofile,"w");
  if (!dsk) {
    fprintf(stderr,"failed to open %s for writing.\n\n",ofile);
    return 3;
  }

  fb = fsize / 512;
  ib = isize / 512;
  rb = rsize / 512;
  memset(buf,0,512);

  /* initialize the full length with zeros */
  tick();
  for(i=0;i<fb;i++)
    if (fwrite(buf, 1, 512, dsk) != 512) goto ioerror;
  
  /* write boot sector */
  if (fseek(dsk,0,SEEK_SET)!=0) goto ioerror;
  bootblock[0x10] = rb;
  tick();
  if (fwrite(bootblock, 1, 512, dsk) != 512) goto ioerror;

  /* prepare superblock */
  memset(&sb, 0, sizeof(sb));
  sb.s_mounted = UZ_SBSIG;
  sb.s_reserv  = rb + 2;
  sb.s_isize   = ib;
  sb.s_fsize   = fb;

  sb.s_tinode  = ib * UZ_IPB - 2;
  
  /* build free list */
  tick();
  j = fb - 1;
  while(j > 2 + rb + ib) {
    if (sb.s_nfree == 50) {
      tick();
      if (fseek(dsk,j*512,SEEK_SET)!=0) goto ioerror;
      if (write_u16(dsk, &(sb.s_nfree), 1)!=0) goto ioerror;
      if (write_u16(dsk, &(sb.s_free[0]), 50)!=0) goto ioerror;
      sb.s_nfree = 0;
      memset(sb.s_free,0,100);
    }
    sb.s_tfree++;
    sb.s_free[sb.s_nfree++] = j--;
  }

  /* write root dir inode (1) */
  memset(&inode, 0, sizeof(inode));
  inode.i_mode    = UZ_IFDIR | 0755;
  inode.i_nlink   = 3;
  inode.i_size    = 16 * 2;
  inode.i_addr[0] = 2 + rb + ib;
  tick();
  if (uz_write_inode(dsk,&sb,UZ_ROOT,&inode)!=0) goto ioerror; 

  /* write root dir data */
  tick();
  if (fseek(dsk,(sb.s_reserv+ib)*512,SEEK_SET)!=0) goto ioerror;
  if (write_u16(dsk,&(rdir[0].d_ino),1)!=0)        goto ioerror;
  if (write_u8(dsk, &(rdir[0].d_name[0]),14)!=0)   goto ioerror;
  if (write_u16(dsk,&(rdir[1].d_ino),1)!=0)        goto ioerror;
  if (write_u8(dsk, &(rdir[1].d_name[0]),14)!=0)   goto ioerror;
  
  /* write reserved inode (0) */
  memset(&inode, 0, sizeof(inode));
  inode.i_nlink = 1;
  inode.i_mode  = ~0;
  tick();
  if (uz_write_inode(dsk,&sb,0,&inode)!=0) goto ioerror; 

  /* free inodes in first inode block */
  j = UZ_ROOT+1;
  while (j < UZ_IPB) {
    if (sb.s_ninode == 50)
      break;
    sb.s_inode[sb.s_ninode++] = j++;
  }
  
  /* write superblock */
  uz_time(&(sb.s_time));
  tick();
  uz_write_sblock(dsk,&sb);
  fclose(dsk);

  if (!quiet)
    printf("\ndone.\n");

  return 0;

 ioerror:
  fprintf(stderr,"** I/O error, creation failed.\n\n");
  return 5;
}
