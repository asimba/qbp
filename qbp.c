#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#ifndef _WIN32
  #include <unistd.h>
#endif

/***********************************************************************************************************/
//Базовая реализация упаковки/распаковки файла по упрощённому алгоритму LZSS+RC32
/***********************************************************************************************************/

#define LZ_BUF_SIZE 257
#define LZ_CAPACITY 24
#define LZ_MIN_MATCH 3

typedef struct{
  int32_t in;
  int32_t out;
} vocpntr;

uint8_t cbuffer[LZ_CAPACITY+1];
uint8_t vocbuf[0x10000];
int32_t vocarea[0x10000];
vocpntr vocindx[0x10000];
uint32_t frequency[257];
uint16_t buf_size;
uint16_t voclast;
uint16_t vocroot;
uint16_t offset;
uint16_t lenght;
uint16_t symbol;
uint32_t low;
uint32_t hlp;
uint32_t range;
uint8_t cflags_count;
uint8_t cbuffer_position;
uint32_t *fc;
char *lowp;
char *hlpp;

void pack_initialize(){
  buf_size=cflags_count=offset=\
    lenght=vocroot=cbuffer[0]=low=hlp=0;
  cbuffer_position=1;
  voclast=0xffff;
  range=0xffffffff;
  lowp=&((char *)&low)[3];
  hlpp=&((char *)&hlp)[0];
  fc=&frequency[256];
  uint32_t i;
  for(i=0;i<257;i++) frequency[i]=i;
  for(i=0;i<0x10000;i++){
    vocarea[i]=vocindx[i].in=vocindx[i].out=-1;
    vocbuf[i]=0;
  };
}

void rc32_rescale(){
  uint16_t i;
  low+=frequency[symbol]*range;
  range*=frequency[symbol+1]-frequency[symbol];
  for(i=symbol+1;i<257;i++) frequency[i]++;
  if(*fc>0xffff){
    frequency[0]>>=1;
    for(i=1;i<257;i++){
      frequency[i]>>=1;
      if(frequency[i]<=frequency[i-1])
        frequency[i]=frequency[i-1]+1;
    };
  };
}

int rc32_read(uint8_t *buf,int l,FILE *ifile){
  while(l--){
    while((range<0x10000)||(hlp<low)){
      if(((low&0xff0000)==0xff0000)&&(range+(uint16_t)low>=0x10000))
        range=0x10000-(uint16_t)low;
      hlp<<=8;
      *hlpp=fgetc(ifile);
      if(ferror(ifile)) return -1;
      if(feof(ifile)) return 1;
      low<<=8;
      range<<=8;
    };
    range/=*fc;
    uint32_t count=(hlp-low)/range;
    if(count>=*fc) return -1;
    for(symbol=255;frequency[symbol]>count;symbol--) if(!symbol) break;
    *buf=(uint8_t)symbol;
    rc32_rescale();
    buf++;
  };
  return 1;
}

int rc32_write(uint8_t *buf,int l,FILE *ofile){
  while(l--){
    while(range<0x10000){
      if(((low&0xff0000)==0xff0000)&&(range+(uint16_t)low>=0x10000))
        range=0x10000-(uint16_t)low;
      fputc(*lowp,ofile);
      if(ferror(ofile)) return -1;
      low<<=8;
      range<<=8;
    };
    symbol=*buf;
    range/=*fc;
    rc32_rescale();
    buf++;
  };
  return 1;
}

void pack_file(FILE *ifile,FILE *ofile){
  char eoff=0,eofs=0;
  vocpntr *indx;
  uint16_t i,tnode,cl;
  int32_t cnode;
  union {uint8_t c[sizeof(uint16_t)];uint16_t i16;} u;
  while(1){
    if(!eoff){
      if(LZ_BUF_SIZE-buf_size){
        symbol=fgetc(ifile);
        if(ferror(ifile)) break;
        if(feof(ifile)) eoff=1;
        else{
          u.c[0]=vocbuf[vocroot];
          u.c[1]=vocbuf[(uint16_t)(vocroot+1)];
          vocindx[u.i16].in=vocarea[vocroot];
          vocarea[vocroot]=-1;
          vocbuf[vocroot]=symbol;
          u.c[0]=vocbuf[voclast];
          u.c[1]=vocbuf[vocroot];
          indx=&vocindx[u.i16];
          if(indx->in>=0) vocarea[indx->out]=voclast;
          else indx->in=voclast;
          indx->out=voclast;
          voclast++;
          vocroot++;
          buf_size++;
          continue;
        };
      };
    };
    lenght=1;
    cbuffer[0]<<=1;
    symbol=vocroot-buf_size;
    if(buf_size){
      if(buf_size>=LZ_MIN_MATCH){
        u.c[0]=vocbuf[symbol];
        u.c[1]=vocbuf[symbol+1];
        cnode=vocindx[u.i16].in;
        while(cnode>=0&&cnode!=symbol&&cnode+lenght!=symbol){
          if(vocbuf[(uint16_t)(symbol+lenght)]==vocbuf[(uint16_t)(cnode+lenght)]){
            tnode=cnode+2;
            cl=2;
            while((cl<buf_size)&&(vocbuf[(uint16_t)(symbol+cl)]==vocbuf[tnode++])) cl++;
            if(cl>=lenght&&cl>2){
              lenght=cl;
              offset=cnode;
            };
            if(lenght==buf_size) break;
          };
          cnode=vocarea[cnode];
        };
      };
      if(lenght>=LZ_MIN_MATCH){
        cbuffer[cbuffer_position++]=lenght-LZ_MIN_MATCH;
        *(uint16_t*)&cbuffer[cbuffer_position++]=offset;
      }
      else{
        cbuffer[0]|=0x01;
        cbuffer[cbuffer_position]=vocbuf[symbol];
      };
      buf_size-=lenght;
    }
    else{
      cbuffer[cbuffer_position]=0xff;
      cbuffer_position+=2;
      if(eoff) eofs=1;
    };
    cbuffer_position++;
    cflags_count++;
    if((cflags_count==8)||eofs){
      cbuffer[0]<<=(8-cflags_count);
      if(rc32_write(cbuffer,cbuffer_position,ofile)<0) break;
      cflags_count=cbuffer[0]=0;
      cbuffer_position=1;
      if(eofs){
        for(i=sizeof(uint32_t);i>0;i--){
          fputc(*lowp,ofile);
          if(ferror(ofile)) return;
          low<<=8;
        }
        break;
      };
    };
  };
  return;
}

void unpack_file(FILE *ifile, FILE *ofile){
  uint16_t i;
  if(!hlp){
    for(i=0;i<sizeof(uint32_t);i++){
      hlp<<=8;
      *hlpp=fgetc(ifile);
      if(ferror(ifile)||feof(ifile)) return;
    }
  };
  while(1){
    if(!cflags_count){
      if(rc32_read(cbuffer,1,ifile)<0) break;
      cflags_count=8;
      cbuffer_position=1;
      lenght=0;
      symbol=cbuffer[0];
      for(i=0;i<cflags_count;i++){
        if(symbol&0x80) lenght++;
        else lenght+=3;
        symbol<<=1;
      };
      if(rc32_read(&cbuffer[1],lenght,ifile)<0) break;
    };
    if(cbuffer[0]&0x80){
      fputc(cbuffer[cbuffer_position],ofile);
      if(ferror(ofile)) break;
      vocbuf[vocroot++]=cbuffer[cbuffer_position];
    }
    else{
      lenght=cbuffer[cbuffer_position++];
      if(lenght==0xff) break;
      lenght+=LZ_MIN_MATCH;
      offset=*(uint16_t*)&cbuffer[cbuffer_position++];
      for(i=0;i<lenght;i++){
        symbol=vocbuf[offset++];
        fputc((uint8_t)symbol,ofile);
        if(ferror(ofile)) return;
        vocbuf[vocroot++]=symbol;
      };
    };
    cbuffer[0]<<=1;
    cbuffer_position++;
    cflags_count--;
  };
  return;
}

/***********************************************************************************************************/

int main(int argc, char *argv[]){
  if((argc<4)||(access(argv[3],F_OK)==0)||\
     ((argv[1][0]!='c')&&(argv[1][0]!='d')))
    goto rpoint00;
  FILE *ifile,*ofile;
  if(!(ifile=fopen(argv[2],"rb"))) goto rpoint00;
  if(!(ofile=fopen(argv[3],"wb"))) goto rpoint01;
  pack_initialize();
  if(argv[1][0]=='c') pack_file(ifile,ofile);
  else unpack_file(ifile,ofile);
  fclose(ofile);
rpoint01:
  fclose(ifile);
rpoint00:
  return 0;
}
