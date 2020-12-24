#ifndef _RC32_H
#define _RC32_H 1

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <fcntl.h>

  /*
    Базовая реализация упаковки/распаковки потока по упрощённому алгоритму Range Coder 32-bit
  */

  typedef struct{
    uint32_t low;
    uint32_t hlp;
    uint32_t range;
    uint32_t *frequency;
    char *lowp;
    char *hlpp;
    char eof;
  } rc32_data;

  void rc32_initialize(rc32_data *rc32d);
  void rc32_release(rc32_data *rc32d);
  int32_t rc32_read(rc32_data *rc32d,void* file,char *buf,int32_t lenght);
  int32_t rc32_write(rc32_data *rc32d,void* file,char *buf,int32_t lenght);

#endif
