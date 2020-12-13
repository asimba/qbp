#ifndef _RC32_H
#define _RC32_H 1

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <fcntl.h>

  /*
    Базовый класс для упаковки/распаковки потока по упрощённому алгоритму Range Coder 32-bit
  */
  typedef int32_t (*io_operator)(void *optional, char *buffer);
  typedef int32_t (*eof_operator)(void *optional);

  class rc32{
    private:
      uint32_t low;
      uint32_t hlp;
      uint32_t range;
      char *lowp;
      char *hlpp;
      io_operator read_op;
      io_operator write_op;
      eof_operator is_eof;
    public:
      uint32_t *frequency;
      bool eof;
      void set_operators(io_operator r, io_operator w, eof_operator e);
      int32_t read(void* file, char *buf, int32_t lenght);
      int32_t write(void* file, char *buf, int32_t lenght);
      rc32();
      ~rc32();
  };

#endif
