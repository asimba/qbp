#include "nvoc.h"

/***********************************************************************************************************/
//Базовый класс циклической очереди поискового словаря алгоритма сжатия.
/***********************************************************************************************************/

nvoc::nvoc(){
  offset=lenght=vocroot=0;
  voclast=0xffff;
  vocarea=(int32_t *)calloc(0x10000,sizeof(int32_t));
  if(vocarea){
    vocindx=(struct vocpntr *)calloc(0x10000,sizeof(struct vocpntr));
    if(vocindx==NULL) free(vocarea);
    else{
      vocbuf=(uint8_t *)calloc(0x10000,sizeof(uint8_t));
      if(vocbuf){
        for(uint32_t i=0;i<0x10000;i++) vocarea[i]=vocindx[i].in=vocindx[i].out=-1;
      }
      else{
        free(vocarea);
        free(vocindx);
      };
    };
  };
}

nvoc::~nvoc(){
  if(vocbuf){
    free(vocarea);
    free(vocindx);
    free(vocbuf);
  };
}

void nvoc::search(uint8_t *str,uint16_t size){
  offset=lenght=0;
  if(size<3) return;
  int32_t cnode=vocindx[*(uint16_t*)str].in;
  bool ds=(str[0]^str[1])?false:true;
  while(cnode>=0){
    if(str[lenght]==vocbuf[(uint16_t)(cnode+lenght)]){
      uint16_t tnode=cnode+2;
      uint16_t cl=2;
      while((cl<size)&&(str[cl]==vocbuf[tnode++])) cl++;
      if(cl>=lenght){
        lenght=cl;
        offset=cnode;
      };
      if(lenght==size) break;
    };
    cnode=vocarea[cnode];
    if(ds&&(cnode>=0)) cnode=vocarea[cnode];
  };
  if(lenght>2) offset-=vocroot;
}

void nvoc::write(uint8_t *str,uint16_t size){
  vocpntr *indx;
  while(size--){
    union {uint8_t c[sizeof(uint16_t)];uint16_t i16;} u;
    u.c[0]=vocbuf[vocroot];
    u.c[1]=vocbuf[(uint16_t)(vocroot+1)];
    vocindx[u.i16].in=vocarea[vocroot];
    vocarea[vocroot]=-1;
    vocbuf[vocroot]=*str++;
    u.c[0]=vocbuf[voclast];
    u.c[1]=vocbuf[vocroot];
    indx=&vocindx[u.i16];
    if(indx->in>=0) vocarea[indx->out]=voclast;
    else indx->in=voclast;
    indx->out=voclast;
    voclast++;
    vocroot++;
  };
}

void nvoc::write_woupdate(uint8_t *str,uint16_t size){
  while(size--) vocbuf[vocroot++]=*str++;
}

void nvoc::read(uint8_t *str,uint16_t index,uint16_t size){
  index+=vocroot;
  while(size--) *str++=vocbuf[index++];
}

/***********************************************************************************************************/
