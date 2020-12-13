#ifndef _LZSS_H
#define _LZSS_H 1

#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>

#include "rc32.h"
#include "nvoc.h"

  #define LZ_BUF_SIZE 258
  #define LZ_CAPACITY 15
  #define LZ_MIN_MATCH 3

  /*
    Базовый класс для упаковки/распаковки потока по упрощённому алгоритму LZSS
  */

  class lzss{
    private:
      uint8_t *cbuffer;
      uint8_t cflags_count;
      uint8_t cbuffer_position;
      uint16_t buf_size;
      rc32 pack;
      nvoc voc;
    public:
      uint8_t *lzbuf;
      void set_operators(io_operator r, io_operator w, eof_operator e);
      int32_t write(void* file, char *buf, int32_t ln);
      int32_t read(void* file, char *buf, int32_t ln);
      int is_eof();
      lzss();
      ~lzss();
  };

#endif
