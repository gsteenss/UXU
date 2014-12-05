
/* 
   Uzix X-Utils (cross platform utilities)   
   (C) 2003 Felipe Bergo - bergo@seul.org 

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License,
   version 2 or (at your option) any later version. The license
   is included in the COPYING file.
*/

/* this defines __BYTE_ORDER to __BIG_ENDIAN or __LITTLE_ENDIAN */
#include <endian.h> 
#include "byteorder.h"

#define BYTE0(a) (a&0xff)
#define BYTE1(a) ((a>>8)&0xff)
#define BYTE2(a) ((a>>16)&0xff)
#define BYTE3(a) ((a>>24)&0xff)

int read_u32(FILE *f, uint32_t *d, int n) {
  size_t r;
#if __BYTE_ORDER == __BIG_ENDIAN
  int i;
  uint32_t a,b;
#endif

  r = fread((void *) d, sizeof(uint32_t), n, f);
  if (r != n)
    return -1;

#if __BYTE_ORDER == __BIG_ENDIAN
  for(i=0;i<n;i++) {
    a = d[i];
    d[i] = (BYTE0(a)<<24) | (BYTE1(a)<<16) | (BYTE2(a)<<8) | BYTE3(a);
  }
#endif

  return 0;
}

int read_s32(FILE *f, int32_t *d, int n) {
  size_t r;
#if __BYTE_ORDER == __BIG_ENDIAN
  int i;
  int32_t a,b;
#endif

  r = fread((void *) d, sizeof(int32_t), n, f);
  if (r != n)
    return -1;

#if __BYTE_ORDER == __BIG_ENDIAN
  for(i=0;i<n;i++) {
    a = d[i];
    d[i] = (BYTE0(a)<<24) | (BYTE1(a)<<16) | (BYTE2(a)<<8) | BYTE3(a);
  }
#endif

  return 0;
}

int read_u16(FILE *f, uint16_t *d, int n) {
  size_t r;
#if __BYTE_ORDER == __BIG_ENDIAN
  int i;
  uint16_t a,b;
#endif

  r = fread((void *) d, sizeof(uint16_t), n, f);
  if (r != n)
    return -1;

#if __BYTE_ORDER == __BIG_ENDIAN
  for(i=0;i<n;i++) {
    a = d[i];
    d[i] = (BYTE0(a)<<8) | BYTE1(a);
  }
#endif

  return 0;
}

int read_s16(FILE *f, int16_t *d, int n) {
  size_t r;
#if __BYTE_ORDER == __BIG_ENDIAN
  int i;
  int16_t a,b;
#endif

  r = fread((void *) d, sizeof(int16_t), n, f);
  if (r != n)
    return -1;

#if __BYTE_ORDER == __BIG_ENDIAN
  for(i=0;i<n;i++) {
    a = d[i];
    d[i] = (BYTE0(a)<<8) | BYTE1(a);
  }
#endif

  return 0;
}

uint16_t u16_to_le(uint16_t x) {
#if __BYTE_ORDER == __BIG_ENDIAN
  uint16_t y;
  y = (BYTE0(x)<<8) | BYTE1(x);
  return y;
#else
  return x;
#endif
}

int32_t  s32_to_le(int32_t x) {
#if __BYTE_ORDER == __BIG_ENDIAN
  int32_t y;
  y = (BYTE0(x)<<24) | (BYTE1(x)<<16) | (BYTE2(x)<<8) | BYTE3(x);
  return y;
#else
  return x;
#endif
}

int read_u8(FILE *f, uint8_t *d, int n) {
  size_t r;

  r = fread((void *) d, sizeof(uint8_t), n, f);
  if (r != n)
    return -1;

  return 0;
}

int read_s8(FILE *f, int8_t *d, int n) {
  size_t r;

  r = fread((void *) d, sizeof(int8_t), n, f);
  if (r != n)
    return -1;

  return 0;
}

int write_s32(FILE *f, int32_t *d, int n) {
  size_t r;
#if __BYTE_ORDER == __BIG_ENDIAN
  int32_t *cnv;
  int i;

  cnv = (int32_t *) malloc(sizeof(int32_t) * n);
  if (!cnv) return -1;

  for(i=0;i<n;i++)
    cnv[i] = s32_to_le(d[i]);
  d = cnv;
#endif
  
  r = fwrite((void *) d, sizeof(int32_t), n, f);
  if (r != n)
    return -1;

#if __BYTE_ORDER == __BIG_ENDIAN
  free(d);
#endif
  return 0;
}

int write_u16(FILE *f, uint16_t *d, int n) {
  size_t r;
#if __BYTE_ORDER == __BIG_ENDIAN
  uint16_t *cnv;
  int i;

  cnv = (uint16_t *) malloc(sizeof(uint16_t) * n);
  if (!cnv) return -1;

  for(i=0;i<n;i++)
    cnv[i] = u16_to_le(d[i]);
  d = cnv;
#endif
  
  r = fwrite((void *) d, sizeof(uint16_t), n, f);
  if (r != n)
    return -1;

#if __BYTE_ORDER == __BIG_ENDIAN
  free(d);
#endif
  return 0;
}

int write_u8(FILE *f, uint8_t *d, int n) {
  size_t r;
  r = fwrite((void *) d, sizeof(uint8_t), n, f);
  if (r != n)
    return -1;
  return 0;
}
