
/* 
   Uzix X-Utils (cross platform utilities)   
   (C) 2003 Felipe Bergo - bergo@seul.org 

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License,
   version 2 or (at your option) any later version. The license
   is included in the COPYING file.
*/

#ifndef BYTEORDER_H
#define BYTEORDER_H 1

#include <stdio.h>
#include <stdint.h>

/* 
   I/O from/to a little-endian file, regardless of the current
   platform
*/

int read_u32(FILE *f, uint32_t *d, int n);
int read_s32(FILE *f, int32_t *d, int n);
int read_u16(FILE *f, uint16_t *d, int n);
int read_s16(FILE *f, int16_t *d, int n);
int read_u8(FILE *f, uint8_t *d, int n);
int read_s8(FILE *f, int8_t *d, int n);

int write_s32(FILE *f, int32_t *d, int n);
int write_u16(FILE *f, uint16_t *d, int n);
int write_u8(FILE *f, uint8_t *d, int n);

uint16_t u16_to_le(uint16_t x);
int32_t  s32_to_le(int32_t x);

#endif
