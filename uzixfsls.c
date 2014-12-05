
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

int flagset(int val, int flag) {
  return( (val & flag) == flag );
}

void listdir(char *path, uz_dir *d) {
  char npath[512];
  char perm[11], tstamp[32];
  uz_direntry cdir;
  uz_stat prop;
  uz_dir  kid;

  printf("%s:\n",path);

  perm[10] = 0;
  while(uz_readdir(d,&cdir)==0) {
    if (!strcmp(cdir.d_name,".") || !strcmp(cdir.d_name,".."))
      continue;

    if (uz_istat(cdir.d_ino, f, &sb, &prop)!=0) {
      printf("can't stat %s, skipping.\n",cdir.d_name);
      continue;
    }

    memset(perm,'-',10);
    
    if (flagset(prop.st_mode,UZ_IFDIR))   perm[0] = 'd';
    if (flagset(prop.st_mode,UZ_IFLNK))   perm[0] = 'l';
    if (flagset(prop.st_mode,UZ_IFBLK))   perm[0] = 'b';
    if (flagset(prop.st_mode,UZ_IFCHR))   perm[0] = 'c';
    if (flagset(prop.st_mode,UZ_IFPIPE))  perm[0] = 'p';
    if (flagset(prop.st_mode,UZ_ISVTX))   perm[0] = 't';

    if (flagset(prop.st_mode,UZ_IREAD))   perm[1] = 'r';
    if (flagset(prop.st_mode,UZ_IWRITE))  perm[2] = 'w';
    if (flagset(prop.st_mode,UZ_IEXEC))   perm[3] = 'x';

    if (flagset(prop.st_mode,UZ_IGREAD))  perm[4] = 'r';
    if (flagset(prop.st_mode,UZ_IGWRITE)) perm[5] = 'w';
    if (flagset(prop.st_mode,UZ_IGEXEC))  perm[6] = 'x';

    if (flagset(prop.st_mode,UZ_IOREAD))  perm[7] = 'r';
    if (flagset(prop.st_mode,UZ_IOWRITE)) perm[8] = 'w';
    if (flagset(prop.st_mode,UZ_IOEXEC))  perm[9] = 'x';
    
    if (flagset(prop.st_mode,UZ_ISUID))  perm[3] = 's';
    if (flagset(prop.st_mode,UZ_ISGID))  perm[6] = 's';
    
    printf("%s %3d %3d:%-3d %8d %19s %s\n",
	   perm, prop.st_nlink, prop.st_uid, prop.st_gid,
	   prop.st_size, uz_date_for_humans(&(prop.st_mtime),tstamp),
	   cdir.d_name);
  }
  printf("\n");

  uz_rewinddir(d);
  while(uz_readdir(d,&cdir)==0) {
    if (!strcmp(cdir.d_name,".") || !strcmp(cdir.d_name,".."))
      continue;
    if (uz_istat(cdir.d_ino, f, &sb, &prop)==0)
      if (flagset(prop.st_mode,UZ_IFDIR)) {
	if (!strcmp(path,"/"))
	  sprintf(npath,"/%s",cdir.d_name);
	else
	  sprintf(npath,"%s/%s",path,cdir.d_name);
	if (uz_opendir(npath,f,&sb,&kid)==0) {
	  listdir(npath, &kid);
	  uz_closedir(&kid);
	}
      }
  }

}

int main(int argc, char **argv) {
  uz_dir d;
  char ipath[512];

  uz_global_opt(argc, argv);

  if (argc!=2 && argc!=3) {
    fprintf(stderr,"usage: uzixfsls image.dsk [directory]\n\n");
    return 1;
  }

  strcpy(ipath,"/");
  if (argc == 3)
    strcpy(ipath, argv[2]);

  f = fopen(argv[1],"r");
  if (!f) {
    fprintf(stderr,"cannot open %s\n\n",argv[1]);
    return 2;
  }

  if (uz_read_sblock(f, &sb) != 0)
    goto err1;
    
  if (uz_opendir(ipath, f, &sb, &d) != 0)
    goto err1;

  listdir(ipath,&d);

  uz_closedir(&d);
  fclose(f);

  return 0;
 err1:
  fprintf(stderr,"error reading image or path does not exist\n\n");
  return 3;
}

