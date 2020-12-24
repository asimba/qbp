#include "lzss.h"

/***********************************************************************************************************/
//Базовая реализация упаковки/распаковки потока по упрощённому алгоритму LZSS
/***********************************************************************************************************/

void lzss_initialize(lzss_data *lzssd){
  lzssd->lzbuf=NULL;
  lzssd->buf_size=lzssd->cflags_count=0;
  lzssd->offset=lzssd->lenght=lzssd->vocroot=0;
  lzssd->cbuffer_position=1;
  lzssd->voclast=0xffff;
  rc32_initialize(&lzssd->pack);
  if(lzssd->pack.frequency){
    lzssd->vocarea=(int32_t *)calloc(0x10000,sizeof(int32_t));
    if(lzssd->vocarea){
      lzssd->vocindx=(vocpntr *)calloc(0x10000,sizeof(vocpntr));
      if(lzssd->vocindx){
        lzssd->vocbuf=(uint8_t *)calloc(0x10000,sizeof(uint8_t));
        if(lzssd->vocbuf){
          for(uint32_t i=0;i<0x10000;i++)
            lzssd->vocarea[i]=lzssd->vocindx[i].in=lzssd->vocindx[i].out=-1;
          lzssd->cbuffer=(uint8_t *)calloc(LZ_CAPACITY+1,sizeof(uint8_t));
          if(lzssd->cbuffer){
            lzssd->cbuffer[0]=0;
            lzssd->lzbuf=(uint8_t *)calloc(LZ_BUF_SIZE*2,sizeof(uint8_t));
          }
        }
      }
    }
  }
  if(!lzssd->lzbuf) lzss_release(lzssd);
}

void lzss_release(lzss_data *lzssd){
  rc32_release(&lzssd->pack);
  if(lzssd->vocarea) free(lzssd->vocarea);
  if(lzssd->vocindx) free(lzssd->vocindx);
  if(lzssd->vocbuf) free(lzssd->vocbuf);
  if(lzssd->cbuffer) free(lzssd->cbuffer);
  if(lzssd->lzbuf) free(lzssd->lzbuf);
}

void lzss_voc_search(lzss_data *lzssd){
  lzssd->offset=lzssd->lenght=0;
  if(lzssd->buf_size<3) return;
  uint8_t *str=lzssd->lzbuf;
  uint16_t size=lzssd->buf_size;
  int32_t cnode=lzssd->vocindx[*(uint16_t*)str].in;
  char ds=(str[0]^str[1])?0:1;
  while(cnode>=0){
    if(str[lzssd->lenght]==lzssd->vocbuf[(uint16_t)(cnode+lzssd->lenght)]){
      uint16_t tnode=cnode+2;
      uint16_t cl=2;
      while((cl<size)&&(str[cl]==lzssd->vocbuf[tnode++])) cl++;
      if(cl>=lzssd->lenght){
        lzssd->lenght=cl;
        lzssd->offset=cnode;
      };
      if(lzssd->lenght==size) break;
    };
    cnode=lzssd->vocarea[cnode];
    if(ds&&(cnode>=0)) cnode=lzssd->vocarea[cnode];
  };
  if(lzssd->lenght>2) lzssd->offset-=lzssd->vocroot;
  else lzssd->lenght=1;
}

void lzss_voc_write(lzss_data *lzssd){
  vocpntr *indx;
  uint8_t *str=lzssd->lzbuf;
  uint16_t size=lzssd->lenght;
  while(size--){
    union {uint8_t c[sizeof(uint16_t)];uint16_t i16;} u;
    u.c[0]=lzssd->vocbuf[lzssd->vocroot];
    u.c[1]=lzssd->vocbuf[(uint16_t)(lzssd->vocroot+1)];
    lzssd->vocindx[u.i16].in=lzssd->vocarea[lzssd->vocroot];
    lzssd->vocarea[lzssd->vocroot]=-1;
    lzssd->vocbuf[lzssd->vocroot]=*str++;
    u.c[0]=lzssd->vocbuf[lzssd->voclast];
    u.c[1]=lzssd->vocbuf[lzssd->vocroot];
    indx=&lzssd->vocindx[u.i16];
    if(indx->in>=0) lzssd->vocarea[indx->out]=lzssd->voclast;
    else indx->in=lzssd->voclast;
    indx->out=lzssd->voclast;
    lzssd->voclast++;
    lzssd->vocroot++;
  };
}

int32_t lzss_write(lzss_data *lzssd,void* file,char *buf,int32_t ln){
  if(!buf) ln+=lzssd->buf_size;
  while(ln){
    if(buf){
      int16_t r=LZ_BUF_SIZE-lzssd->buf_size;
      if(r){
        if(r>ln) r=ln;
        memcpy(lzssd->lzbuf+lzssd->buf_size,buf,r);
        lzssd->buf_size+=r;
        buf+=r;
        ln-=r;
      };
    }
    else ln-=lzssd->buf_size;
    if((buf)&&(ln==0)) break;
    lzss_voc_search(lzssd);
    if(lzssd->cflags_count==5){
      lzssd->cbuffer[0]|=0xa0;
      if(rc32_write(&lzssd->pack,file,(char*)lzssd->cbuffer,lzssd->cbuffer_position)<0) return -1;
      lzssd->cflags_count=0;
      lzssd->cbuffer[0]=0;
      lzssd->cbuffer_position=1;
    };
    lzssd->cbuffer[0]<<=1;
    if(lzssd->lenght>=LZ_MIN_MATCH){
      lzssd->cbuffer[lzssd->cbuffer_position++]=(uint8_t)(lzssd->lenght-LZ_MIN_MATCH);
      *(uint16_t*)&lzssd->cbuffer[lzssd->cbuffer_position++]=lzssd->offset;
    }
    else{
      lzssd->cbuffer[0]|=0x01;
      lzssd->cbuffer[lzssd->cbuffer_position]=lzssd->lzbuf[0];
    };
    lzssd->cbuffer_position++;
    lzssd->cflags_count++;
    lzss_voc_write(lzssd);
    memmove(lzssd->lzbuf,lzssd->lzbuf+lzssd->lenght,LZ_BUF_SIZE-lzssd->lenght);
    lzssd->buf_size-=lzssd->lenght;
    if(!buf) ln+=lzssd->buf_size;
  };
  if(!buf&&!lzssd->buf_size){
    if(lzssd->cflags_count){
      lzssd->cbuffer[0]<<=(5-lzssd->cflags_count);
      lzssd->cbuffer[0]|=(lzssd->cflags_count<<5);
      if(rc32_write(&lzssd->pack,file,(char*)lzssd->cbuffer,lzssd->cbuffer_position)<0) return -1;
    };
    while(!lzssd->pack.eof){
      if(rc32_write(&lzssd->pack,file,NULL,0)<0) return -1;
    };
  };
  return 1;
}

int32_t lzss_read(lzss_data *lzssd,void* file,char *buf,int32_t ln){
  int32_t rl=0;
  uint16_t c=0;
  if(lzssd->pack.eof){
    lzssd->buf_size=0;
    return 0;
  };
  while(ln){
    if(lzssd->buf_size){
      int32_t r=(ln>lzssd->buf_size)?lzssd->buf_size:ln;
      memcpy(buf,lzssd->lzbuf,r);
      memmove(lzssd->lzbuf,lzssd->lzbuf+r,LZ_BUF_SIZE-r);
      lzssd->buf_size-=r;
      buf+=r;
      ln-=r;
      rl+=r;
    }
    else{
      if(lzssd->cflags_count==0){
        if(rc32_read(&lzssd->pack,file,(char*)lzssd->cbuffer,1)<0) return -1;
        lzssd->cflags_count=lzssd->cbuffer[0]>>5;
        lzssd->cbuffer[0]<<=3;
        if(lzssd->pack.eof||(lzssd->cflags_count==0)||(lzssd->cflags_count==6)||(lzssd->cflags_count==7))
          return rl;
        lzssd->cbuffer_position=1;
        c=0;
        uint8_t f=lzssd->cbuffer[0];
        for(uint8_t cf=0; cf<lzssd->cflags_count; cf++){
          if(f&0x80) c++;
          else c+=3;
          f<<=1;
        };
        if(rc32_read(&lzssd->pack,file,(char*)(&lzssd->cbuffer[1]),c)<0)
          return -1;
      };
      if(lzssd->cbuffer[0]&0x80){
        lzssd->lzbuf[0]=lzssd->cbuffer[lzssd->cbuffer_position];
        lzssd->vocbuf[lzssd->vocroot++]=lzssd->cbuffer[lzssd->cbuffer_position];
        lzssd->buf_size=1;

      }
      else{
        c=lzssd->cbuffer[lzssd->cbuffer_position++]+LZ_MIN_MATCH;
        lzssd->buf_size=*(uint16_t*)&lzssd->cbuffer[lzssd->cbuffer_position++]+lzssd->vocroot;
        for(uint16_t i=0; i<c; i++){
          lzssd->lzbuf[i]=lzssd->vocbuf[lzssd->buf_size++];
        }
        for(uint16_t i=0; i<c; i++){
          lzssd->vocbuf[lzssd->vocroot++]=lzssd->lzbuf[i];
        };
        lzssd->buf_size=c;
      };
      lzssd->cbuffer[0]<<=1;
      lzssd->cbuffer_position++;
      lzssd->cflags_count--;
    };
  };
  return rl;
}

int lzss_is_eof(lzss_data *lzssd){
  return ((!lzssd->buf_size)&&(lzssd->pack.eof))?1:0;
}

/***********************************************************************************************************/
