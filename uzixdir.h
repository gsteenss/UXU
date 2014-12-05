
/* 
   Uzix X-Utils (cross platform utilities)   
   (C) 2003 Felipe Bergo - bergo@seul.org 

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License,
   version 2 or (at your option) any later version. The license
   is included in the COPYING file.
*/

#ifndef UZIXDIR_H
#define UZIXDIR_H 1

#include <stdio.h>
#include "uzixfs.h"

/* frills that shouldn't pollute uzixfs.c */

typedef struct {
  int       count;
  int       next;
  uz_inode  inode;
  uz_sblock *sb;
  FILE      *dsk;
} uz_dir;

typedef struct {
  uz_ino_t   st_ino;
  uz_mode_t  st_mode;
  uint16_t   st_nlink;       /* number of links to file */
  uint8_t    st_uid, st_gid; /* file owner */
  uz_off_t   st_size;        /* file size  */
  uz_time_t  st_atime;       /* last access time       */
  uz_time_t  st_mtime;       /* last modification time */
  uz_time_t  st_ctime;       /* file creation time     */
} uz_stat;

int  uz_opendir(char *path, FILE *f, uz_sblock *sb, uz_dir *dir);
int  uz_openrootdir(FILE *f, uz_sblock *sb, uz_dir *root);
int  uz_iopendir(uz_ino_t inode, FILE *f, uz_sblock *sb, uz_dir *dir);

void uz_rewinddir(uz_dir *dir);
int  uz_readdir(uz_dir *dir, uz_direntry *dentry);
void uz_closedir(uz_dir *dir);

/* returns the inode for the given path name, -1 on error */
int  uz_lookup(char *path, FILE *f, uz_sblock *sb);

int  uz_fstat(char *path, FILE *f, uz_sblock *sb, uz_stat *ostat);
int  uz_istat(uz_ino_t inode, FILE *f, uz_sblock *sb, uz_stat *ostat);


/* common behavior to all utilities (-v and --version) */
void uz_global_opt(int argc, char **argv);

#endif
