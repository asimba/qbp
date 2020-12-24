#ifndef _LZSS_H
#define _LZSS_H 1

#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "rc32.h"

  #define LZ_BUF_SIZE 258
  #define LZ_CAPACITY 15
  #define LZ_MIN_MATCH 3

  /*
    Базовая реализация упаковки/распаковки потока по упрощённому алгоритму LZSS
  */

  typedef struct{
    int32_t in;
    int32_t out;
  } vocpntr;

  typedef struct{
    uint8_t *cbuffer;
    uint8_t *lzbuf;
    uint8_t *vocbuf;
    uint8_t cflags_count;
    uint8_t cbuffer_position;
    uint16_t buf_size;
    int32_t *vocarea;
    uint16_t voclast;
    uint16_t vocroot;
    uint16_t offset;
    uint16_t lenght;
    vocpntr *vocindx;
    rc32_data pack;
  } lzss_data;

  void lzss_initialize(lzss_data *lzssd);
  void lzss_release(lzss_data *lzssd);
  int32_t lzss_write(lzss_data *lzssd,void* file,char *buf,int32_t ln);
  int32_t lzss_read(lzss_data *lzssd,void* file,char *buf,int32_t ln);
  int lzss_is_eof(lzss_data *lzssd);

#endif
