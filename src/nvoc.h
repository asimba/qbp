#ifndef __NVOC_H
#define __NVOC_H 1

#include <stdint.h>
#include <string.h>
#include <malloc.h>

  struct vocpntr{
    int32_t in;
    int32_t out;
  };

  class nvoc{
    private:
      int32_t *vocarea;
      uint8_t *vocbuf;
      uint16_t vocroot;
      uint16_t voclast;
    public:
      struct vocpntr *vocindx;
      uint16_t offset;
      uint16_t lenght;
      void search(uint8_t *str, uint16_t size);
      void write(uint8_t *str, uint16_t size);
      void write_woupdate(uint8_t *str, uint16_t size);
      void read(uint8_t *str, uint16_t index, uint16_t size);
      nvoc();
      ~nvoc();
  };

#endif
