#include "rc32.h"

/***********************************************************************************************************/
//Базовая реализация упаковки/распаковки потока по упрощённому алгоритму Range Coder 32-bit
/***********************************************************************************************************/

void rc32_initialize(rc32_data *rc32d){
  rc32d->range=0xffffffff;
  rc32d->low=rc32d->hlp=0;
  rc32d->lowp=&((char *)&rc32d->low)[3];
  rc32d->hlpp=&((char *)&rc32d->hlp)[0];
  rc32d->eof=0;
  rc32d->frequency=(uint32_t *)calloc(257,sizeof(uint32_t));
  if(rc32d->frequency) for(uint16_t i=0; i<257; i++) rc32d->frequency[i]=i;
}

void rc32_release(rc32_data *rc32d){
  if(rc32d->frequency) free(rc32d->frequency);
}

void rc32_rescale(rc32_data *rc32d,uint16_t i){
  uint16_t j;
  rc32d->low+=rc32d->frequency[i]*rc32d->range;
  rc32d->range*=rc32d->frequency[i+1]-rc32d->frequency[i];
  for(j=i+1; j<257; j++) rc32d->frequency[j]++;
  if(rc32d->frequency[256]>0xffff){
    rc32d->frequency[0]>>=1;
    for(j=1; j<257; j++){
      rc32d->frequency[j]>>=1;
      if(rc32d->frequency[j]<=rc32d->frequency[j-1])
        rc32d->frequency[j]=rc32d->frequency[j-1]+1;
    };
  };
}

int32_t rc32_read(rc32_data *rc32d,void* file,char *buf,int32_t lenght){
  if(!rc32d->hlp){
    for(uint16_t i=0;i<sizeof(uint32_t);i++){
      rc32d->hlp<<=8;
      *rc32d->hlpp=fgetc((FILE*)file);
      if(ferror((FILE*)file)||feof((FILE*)file)){
        rc32d->hlp=0;
        return -1;
      };
    }
  };
  while(lenght--){
    if(!rc32d->hlp){
      rc32d->eof=1;
      return 1;
    };
    uint16_t i;
    rc32d->range/=rc32d->frequency[256];
    uint32_t count=(rc32d->hlp-rc32d->low)/rc32d->range;
    if(count>=rc32d->frequency[256]) return -1;
    for(i=255; rc32d->frequency[i]>count; i--) if(!i) break;
    *buf=(uint8_t)i;
    rc32_rescale(rc32d,i);
    while((rc32d->range<0x10000)||(rc32d->hlp<rc32d->low)){
      if(((rc32d->low&0xff0000)==0xff0000)&&(rc32d->range+(uint16_t)rc32d->low>=0x10000))
        rc32d->range=0x10000-(uint16_t)rc32d->low;
      rc32d->hlp<<=8;
      *rc32d->hlpp=fgetc((FILE*)file);
      if(ferror((FILE*)file)) return -1;
      if(feof((FILE*)file)) rc32d->hlp=0;
      if(!rc32d->hlp) break;
      rc32d->low<<=8;
      rc32d->range<<=8;
    };
    buf++;
  };
  return 1;
}

int32_t rc32_write(rc32_data *rc32d,void* file,char *buf,int32_t lenght){
  if((!lenght)&&(!buf)){
    for(int i=sizeof(uint32_t);i>0;i--){
      fputc(*rc32d->lowp,(FILE*)file);
      if(ferror((FILE*)file)) return -1;
      rc32d->low<<=8;
    }
    rc32d->eof=1;
  }
  else while(lenght--){
    uint16_t i=(uint8_t)*buf;
    rc32d->range/=rc32d->frequency[256];
    rc32_rescale(rc32d,i);
    while(rc32d->range<0x10000){
      if(((rc32d->low&0xff0000)==0xff0000)&&(rc32d->range+(uint16_t)rc32d->low>=0x10000))
        rc32d->range=0x10000-(uint16_t)rc32d->low;
      fputc(*rc32d->lowp,(FILE*)file);
      if(ferror((FILE*)file)) return -1;
      rc32d->low<<=8;
      rc32d->range<<=8;
    };
    buf++;
  };
  return 1;
}

/***********************************************************************************************************/
