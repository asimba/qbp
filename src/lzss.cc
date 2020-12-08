#include "lzss.h"

/***********************************************************************************************************/
//Базовый класс для упаковки/распаковки потока по упрощённому алгоритму LZSS
/***********************************************************************************************************/

lzss::lzss(){
  lzbuf=NULL;
  if((pack.frequency==NULL)||(voc.vocindx==NULL)) return;
  cbuffer=(uint8_t *)calloc(LZ_CAPACITY+1,sizeof(uint8_t));
  if(cbuffer){
    cbuffer[0]=cflags_count=0;
    cbuffer_position=1;
  }
  else return;
  lzbuf_pntr=lzbuf=(uint8_t *)calloc(LZ_BUF_SIZE*2,sizeof(uint8_t));
  if(lzbuf){
    lzbuf_indx=buf_size=0;
    op_code=true;
    finalize=false;
  }
  else free(cbuffer);
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
  int32_t retln=0;
  uint32_t c;
  if(finalize) ln+=buf_size;
  while(ln){
    retln=LZ_BUF_SIZE-buf_size;
    if(finalize==false){
      if(retln!=0){
        if(retln>ln) retln=ln;
        memcpy(lzbuf_pntr+buf_size,buf,retln);
        buf+=retln;
        ln-=retln;
        buf_size+=retln;
      }
    }
    else ln-=buf_size;
    if((finalize==false)&&(ln==0)) break;
    voc.search(lzbuf_pntr,buf_size);
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
      cbuffer[cbuffer_position]=lzbuf_pntr[0];
    };
    cbuffer_position++;
    cflags_count++;
    voc.write(lzbuf_pntr,c);
    if((lzbuf_indx+c)>=LZ_BUF_SIZE){
      memmove(lzbuf,lzbuf_pntr+c,LZ_BUF_SIZE-c);
      lzbuf_pntr=lzbuf;
      lzbuf_indx=0;
    }
    else{
      lzbuf_pntr=&lzbuf_pntr[c];
      lzbuf_indx+=c;
    };
    buf_size-=c;
  };
  return 1;
}

int32_t lzss::lzss_read(void* file, char *buf, int32_t ln){
  int32_t retln;
  int32_t rl=0;
  uint8_t f=0;
  uint16_t c=0;
  uint16_t o=0;
  if(pack.eof) return 0;
  while(ln){
    if(buf_size){
      retln=(ln>buf_size)?buf_size:ln;
      memcpy(buf,lzbuf,retln);
      memmove(lzbuf,lzbuf+retln,LZ_BUF_SIZE-retln);
      buf+=retln;
      ln-=retln;
      rl+=retln;
      buf_size-=retln;
    }
    else{
      if(cflags_count==0){
        if(pack.rc32_read(file,(char*)cbuffer,1)<0) return -1;
        cflags_count=cbuffer[0]>>5;
        cbuffer[0]<<=3;
        if(pack.eof||(cflags_count==0)||(cflags_count==7)) return rl;
        cbuffer_position=1;
        c=0;
        f=cbuffer[0];
        for(int cf=0; cf<cflags_count; cf++){
          if(f&0x80) c++;
          else c+=3;
          f<<=1;
        };
        if(pack.rc32_read(file,(char*)(&cbuffer[1]),c)<0) return -1;
      };
      if(cbuffer[0]&0x80){
        lzbuf[0]=cbuffer[cbuffer_position++];
        voc.write_woupdate(lzbuf,1);
        buf_size=1;

      }
      else{
        c=cbuffer[cbuffer_position++]+LZ_MIN_MATCH;
        o=*(uint16_t*)&cbuffer[cbuffer_position];
        cbuffer_position+=sizeof(uint16_t);
        voc.read(lzbuf,o,c);
        voc.write_woupdate(lzbuf,c);
        buf_size=c;
      };
      cbuffer[0]<<=1;
      cflags_count--;
      if(finalize) break;
    };
  };
  return rl;
}

int lzss::is_eof(FILE* file){
  if(finalize&&(buf_size==0)){
    if(op_code){
      if(cflags_count){
        cbuffer[0]|=(cflags_count<<5);
        if(pack.rc32_write(file,(char*)cbuffer,cbuffer_position)<0) return -1;
      };
      pack.finalize=true;
      while(pack.eof==false){
        if(pack.rc32_write(file,NULL,0)<0) return -1;
      };
      return 1;
    }
    else return 1;
  }
  else return 0;
}

/***********************************************************************************************************/
