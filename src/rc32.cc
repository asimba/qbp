#include "rc32.h"

/***********************************************************************************************************/
//Базовый класс для упаковки/распаковки потока по упрощённому алгоритму Range Coder 32-bit
/***********************************************************************************************************/

void rc32::set_operators(io_operator r, io_operator w, eof_operator e){
  if(r) read=r;
  if(w) write=w;
  if(e) is_eof=e;
}

int32_t rc32::rc32_read(void* file, char *buf, int32_t lenght){
  if((!read)||(!is_eof)){
    eof=true;
    return -1;
  }
  if(!hlp){
    if((bufsize=(*read)(file,(char *)pqbuffer,_RC32_BUFFER_SIZE))<=0)
      return -1;
    for(uint16_t i=0;i<sizeof(uint32_t);i++){
      hlp<<=8;
      hlp|=pqbuffer[rbufsize++];
      bufsize--;
    }
  };
  while(lenght--){
    if(!hlp){
      eof=true;
      return 1;
    };
    uint16_t i;
    range/=frequency[256];
    uint32_t count=(hlp-low)/range;
    if(count>=frequency[256]) return -1;
    for(i=255; frequency[i]>count; i--) if(!i) break;
    *buf=(uint8_t)i;
    low+=frequency[i]*range;
    range*=frequency[i+1]-frequency[i];
    for(i=(uint8_t)*buf+1; i<257; i++) frequency[i]++;
    if(frequency[256]>=0x10000){
      frequency[0]>>=1;
      for(i=1; i<257; i++){
        frequency[i]>>=1;
        if(frequency[i]<=frequency[i-1]) frequency[i]=frequency[i-1]+1;
      };
    };
    while((range<0x10000)||(hlp<low)){
      if(((low&0xff0000)==0xff0000)&&(range+(uint16_t)low>=0x10000))
        range=0x10000-(uint16_t)low;
      hlp<<=8;
      if(bufsize==0){
        if((bufsize=(*read)(file,(char *)pqbuffer,_RC32_BUFFER_SIZE))<0)
          return -1;
        if((bufsize==0)&&(*is_eof)(file)) hlp=0;
        rbufsize=0;
      };
      if(bufsize){
        hlp|=pqbuffer[rbufsize++];
        bufsize--;
      };
      if(!hlp) break;
      low<<=8;
      range<<=8;
    };
    buf++;
  };
  return 1;
}

int32_t rc32::rc32_write(void* file, char *buf, int32_t lenght){
  if(!write){
    eof=true;
    return -1;
  }
  if((!lenght)&&(!buf)){
    if(bufsize&&((*write)(file,(char *)pqbuffer,bufsize)==0))
      return -1;
    bufsize=0;
    for(int i=sizeof(uint32_t);i>0;i--){
      pqbuffer[bufsize++]=(uint8_t)(low>>24);
      low<<=8;
    }
    if((*write)(file,(char *)pqbuffer,bufsize)==0)
      return -1;
    eof=true;
  }
  else while(lenght--){
    uint16_t i;
    range/=frequency[256];
    low+=frequency[(uint8_t)*buf]*range;
    range*=frequency[(uint8_t)*buf+1]-frequency[(uint8_t)*buf];
    for(i=(uint8_t)*buf+1; i<257; i++) frequency[i]++;
    if(frequency[256]>=0x10000){
      frequency[0]>>=1;
      for(i=1; i<257; i++){
        frequency[i]>>=1;
        if(frequency[i]<=frequency[i-1]) frequency[i]=frequency[i-1]+1;
      };
    };
    while(range<0x10000){
      if(((low&0xff0000)==0xff0000)&&(range+(uint16_t)low>=0x10000))
        range=0x10000-(uint16_t)low;
      if(bufsize==_RC32_BUFFER_SIZE){
        if((*write)(file,(char *)pqbuffer,bufsize)==0)
          return -1;
        bufsize=0;
      };
      pqbuffer[bufsize++]=(uint8_t)(low>>24);
      low<<=8;
      range<<=8;
    };
    buf++;
  };
  return 1;
}

rc32::rc32(){
  pqbuffer=(uint8_t *)calloc(_RC32_BUFFER_SIZE,sizeof(uint8_t));
  if(!pqbuffer) return;
  frequency=(uint32_t *)calloc(257,sizeof(uint32_t));
  if(frequency) for(low=0; low<257; low++) frequency[low]=low;
  else return;
  range=0xffffffff;
  low=bufsize=rbufsize=hlp=0;
  eof=false;
  read=write=NULL;
  is_eof=NULL;
}

rc32::~rc32(){
  if(frequency){
    free(frequency);
    free(pqbuffer);
  };
}

/***********************************************************************************************************/
