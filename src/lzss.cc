#include "lzss.h"

/***********************************************************************************************************/
//Базовый класс для упаковки/распаковки потока по упрощённому алгоритму LZSS
/***********************************************************************************************************/

lzss::lzss(){
  lzbuf=NULL;
  buf_size=cbuffer[0]=cflags_count=0;
  cbuffer_position=1;
  if((!pack.frequency)||(!voc.vocindx)) return;
  cbuffer=(uint8_t *)calloc(LZ_CAPACITY+1,sizeof(uint8_t));
  if(!cbuffer) return;
  lzbuf=(uint8_t *)calloc(LZ_BUF_SIZE*2,sizeof(uint8_t));
  if(!lzbuf) free(cbuffer);
}

lzss::~lzss(){
  if(lzbuf){
    free(cbuffer);
    free(lzbuf);
  };
}

void lzss::set_operators(io_operator r, io_operator w, eof_operator e){
  if(r&&w&&e) pack.set_operators(r,w,e);
}

int32_t lzss::lzss_write(void* file, char *buf, int32_t ln){
  uint32_t c;
  if(!buf) ln+=buf_size;
  while(ln){
    if(buf){
      int16_t r=LZ_BUF_SIZE-buf_size;
      if(r){
        if(r>ln) r=ln;
        memcpy(lzbuf+buf_size,buf,r);
        buf_size+=r;
        buf+=r;
        ln-=r;
      };
    }
    else ln-=buf_size;
    if((buf)&&(ln==0)) break;
    voc.search(lzbuf,buf_size);
    if(cflags_count==5){
      cbuffer[0]|=0xa0;
      if(pack.rc32_write(file,(char*)cbuffer,cbuffer_position)<0) return -1;
      cflags_count=0;
      cbuffer[0]=0;
      cbuffer_position=1;
    };
    cbuffer[0]<<=1;
    if(voc.lenght>=LZ_MIN_MATCH){
      c=voc.lenght;
      cbuffer[cbuffer_position++]=(uint8_t)(c-LZ_MIN_MATCH);
      *(uint16_t*)&cbuffer[cbuffer_position++]=voc.offset;
    }
    else{
      c=1;
      cbuffer[0]|=0x01;
      cbuffer[cbuffer_position]=lzbuf[0];
    };
    cbuffer_position++;
    cflags_count++;
    voc.write(lzbuf,c);
    memmove(lzbuf,lzbuf+c,LZ_BUF_SIZE-c);
    buf_size-=c;
  };
  if(!buf){
    if(cflags_count){
      cbuffer[0]|=(cflags_count<<5);
      if(pack.rc32_write(file,(char*)cbuffer,cbuffer_position)<0) return -1;
    };
    while(!pack.eof){
      if(pack.rc32_write(file,NULL,0)<0) return -1;
    };
  };
  return 1;
}

int32_t lzss::lzss_read(void* file, char *buf, int32_t ln){
  int32_t rl=0;
  uint16_t c=0;
  if(pack.eof) return 0;
  while(ln){
    if(buf_size){
      int32_t r=(ln>buf_size)?buf_size:ln;
      memcpy(buf,lzbuf,r);
      memmove(lzbuf,lzbuf+r,LZ_BUF_SIZE-r);
      buf_size-=r;
      buf+=r;
      ln-=r;
      rl+=r;
    }
    else{
      if(cflags_count==0){
        if(pack.rc32_read(file,(char*)cbuffer,1)<0) return -1;
        cflags_count=cbuffer[0]>>5;
        cbuffer[0]<<=3;
        if(pack.eof||(cflags_count==0)||(cflags_count==7)) return rl;
        cbuffer_position=1;
        c=0;
        uint8_t f=cbuffer[0];
        for(uint8_t cf=0; cf<cflags_count; cf++){
          if(f&0x80) c++;
          else c+=3;
          f<<=1;
        };
        if(pack.rc32_read(file,(char*)(&cbuffer[1]),c)<0) return -1;
      };
      if(cbuffer[0]&0x80){
        lzbuf[0]=cbuffer[cbuffer_position];
        voc.vocbuf[voc.vocroot++]=cbuffer[cbuffer_position];
        buf_size=1;

      }
      else{
        c=cbuffer[cbuffer_position++]+LZ_MIN_MATCH;
        buf_size=*(uint16_t*)&cbuffer[cbuffer_position++]+voc.vocroot;
        for(uint16_t i=0; i<c; i++){
          lzbuf[i]=voc.vocbuf[buf_size++];
        }
        for(uint16_t i=0; i<c; i++){
          voc.vocbuf[voc.vocroot++]=lzbuf[i];
        };
        buf_size=c;
      };
      cbuffer[0]<<=1;
      cbuffer_position++;
      cflags_count--;
    };
  };
  return rl;
}

int lzss::is_eof(){
  return ((!buf_size)&&(pack.eof))?1:0;
}

/***********************************************************************************************************/
