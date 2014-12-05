
/* 
   Uzix X-Utils (cross platform utilities)   
   (C) 2003 Felipe Bergo - bergo@seul.org 

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License,
   version 2 or (at your option) any later version. The license
   is included in the COPYING file.
*/

#include <stdlib.h>
#include <string.h>
#include "uzixdir.h"

int  uz_iopendir(uz_ino_t inode, FILE *f, uz_sblock *sb, uz_dir *dir) {
  dir->sb   = sb;
  dir->next = 0;
  dir->dsk  = f;

  if (uz_read_inode(dir->dsk,dir->sb,inode,&(dir->inode)) != 0)
    return -1;

  if ( (dir->inode.i_mode & UZ_IFDIR) == 0)
    return -1; /* target is not a directory */

  /* actually this is wrong because file sizes are reported on block
     boundaries due to some odd bug in uzix or its fs utilities */
  dir->count = dir->inode.i_size / UZ_DIRELEN;
  return 0;
}

int  uz_openrootdir(FILE *f,uz_sblock *sb, uz_dir *root) {
  return(uz_iopendir(UZ_ROOT,f,sb,root));
}

int  uz_opendir(char *path, FILE *f, uz_sblock *sb, uz_dir *dir) {  
  int i;

  i = uz_lookup(path,f,sb);
  if (i<0) return -1;

  return(uz_iopendir(i,f,sb,dir));
}

void uz_rewinddir(uz_dir *dir) {
  dir->next = 0;
}

int  uz_readdir(uz_dir *dir, uz_direntry *dentry) {
  if (dir->next >= dir->count)
    return -1;
  if (uz_read_data(dir->dsk,&(dir->inode), 
		   UZ_DIRELEN * dir->next, UZ_DIRELEN, (void *) dentry) < 0)
    return -1;

  if (dentry->d_ino == 0) {
    dir->count = dir->next;
    return -1;
  }

  ++(dir->next);
  return 0;
}

void uz_closedir(uz_dir *dir) {
  /* nothing needs to be done */
}

int  uz_istat(uz_ino_t inode, FILE *f, uz_sblock *sb, uz_stat *ostat) {
  uz_inode xinode;

  if (uz_read_inode(f,sb,inode,&xinode) != 0)
    return -1;

  ostat->st_ino   = inode;
  ostat->st_mode  = xinode.i_mode;
  ostat->st_nlink = xinode.i_nlink;
  ostat->st_uid   = xinode.i_uid;
  ostat->st_gid   = xinode.i_gid;
  ostat->st_size  = xinode.i_size;
  memcpy(&(ostat->st_atime),&(xinode.i_atime),sizeof(uz_time_t));
  memcpy(&(ostat->st_mtime),&(xinode.i_mtime),sizeof(uz_time_t));
  memcpy(&(ostat->st_ctime),&(xinode.i_ctime),sizeof(uz_time_t));

  return 0;
}

int  uz_fstat(char *path, FILE *f, uz_sblock *sb, uz_stat *ostat) {
  int i;

  i = uz_lookup(path, f, sb);
  if (i<0) return -1;

  return(uz_istat(i,f,sb,ostat));
}

/* FIXME: does not follow symlinks yet */
int  uz_lookup(char *path, FILE *f, uz_sblock *sb) {
  char        pelem[16];
  int         i,j;
  uz_dir      parent;
  uz_ino_t    cnode, nnode;
  uz_direntry entry;

  if (uz_openrootdir(f,sb,&parent) != 0)
    return -1;

  cnode = UZ_ROOT;
  i=0;

  for(;;) {
    j=0; memset(pelem,0,16);
    while(path[i]=='/' && path[i]!=0) ++i;

    if (path[i] == 0) {
      uz_closedir(&parent);
      return cnode;
    }

    while(path[i]!='/' && path[i]!=0) pelem[j++] = path[i++];
    
    /* find dir entry that matches pelem */
    nnode = 0;
    while(uz_readdir(&parent,&entry)==0) {
      if (!strcmp(pelem, entry.d_name)) {
	nnode = entry.d_ino;
	break;
      }      
    }
    if (nnode == 0)
      return -1; /* path element not found */
    uz_closedir(&parent);
    
    cnode = nnode;
    if (path[i] == 0) /* this was the last path element, return inode # */
      return cnode;

    if (uz_iopendir(cnode, f, sb, &parent) != 0)
      return -1;
  }

}

void uz_global_opt(int argc, char **argv) {
  int i;
  for(i=1;i<argc;i++) {
    if (!strcmp(argv[i],"-v") || !strcmp(argv[i],"--version")) {
      fprintf(stderr,"Uzix X-Utils (Uzix Cross Platform Utilities) version 1.0\n");
      fprintf(stderr,"(C) 2003 Felipe Bergo <bergo@seul.org>. This is free\n");
      fprintf(stderr,"software under the terms of the GNU General Public License.\n\n");
      exit(9);
    }
  }
}
