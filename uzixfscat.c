/* 
   Uzix X-Utils (cross platform utilities)   
   (C) 2003 Felipe Bergo - bergo@seul.org 

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License,
   version 2 or (at your option) any later version. The license
   is included in the COPYING file.
*/

#include <stdio.h>
#include <string.h>
#include "uzixfs.h"
#include "uzixdir.h"


FILE *f;
uz_sblock sb;

int main(int argc, char **argv) {
  uz_inode inode;
  int i, j, ino, sz, blks, co, cr;
  char blk[512];

  uz_global_opt(argc, argv);

  if (argc!=3) {
    fprintf(stderr,"usage: uzixfscat image.dsk file\n\n");
    return 1;
  }

  f = fopen(argv[1],"r");
  if (!f) {
    fprintf(stderr,"cannot open %s\n\n",argv[1]);
    return 2;
  }

  if (uz_read_sblock(f, &sb) != 0)
    goto err1;
    
  ino = uz_lookup(argv[2],f,&sb);

  if (ino<0) {
    fprintf(stderr,"file not found on given image.\n");
    fclose(f);
    return 4;
  }

  if (uz_read_inode(f, &sb, ino, &inode)!=0)
    goto err1;
  
  sz = inode.i_size;
  blks = uz_fit_bytes(sz);

  co = 0;
  for(i=0;i<blks;i++) {
    memset(blk,0,512);
    j = uz_xlate_block(f, &inode, i);
    if (j<0) goto err1;

    cr = sz-co;
    if (cr > 512) cr=512;
    if (uz_read_raw_block(f,j,(void *)blk)!=0) goto err1;
    co+=cr;

    if (fwrite(blk,1,cr,stdout) != cr)
      goto err1;
  }

  fclose(f);
  return 0;
 err1:
  fprintf(stderr,"error reading image or path does not exist\n\n");
  return 3;
}
