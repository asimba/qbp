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

#define LZ_BUF_SIZE 259
#define LZ_CAPACITY 24
#define LZ_MIN_MATCH 3

typedef struct{
  int32_t in;
  int32_t out;
} vocpntr;

uint8_t flags;
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
uint32_t *fc;
char *lowp;
char *hlpp;

void pack_initialize(){
  buf_size=flags=vocroot=*cbuffer=low=hlp=0;
  voclast=range=0xffffffff;
  lowp=&((char *)&low)[3];
  hlpp=&((char *)&hlp)[0];
  fc=&frequency[256];
  uint32_t i;
  for(i=0;i<257;i++) frequency[i]=i;
  for(i=0;i<0x10000;i++)
    vocbuf[i]=vocarea[i]=vocindx[i].in=vocindx[i].out=-1;
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
  union {uint8_t c[sizeof(uint16_t)];uint16_t i16;} u;
  register uint16_t i;
  char eoff=0,eofs=0;
  vocpntr *indx;
  int32_t cnode;
  uint8_t *cpos=&cbuffer[1];
  flags=8;
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
    *cbuffer<<=1;
    symbol=vocroot-buf_size;
    if(buf_size){
      lenght=LZ_MIN_MATCH;
      if(buf_size>LZ_MIN_MATCH){
        u.c[0]=vocbuf[symbol];
        u.c[1]=vocbuf[symbol+1];
        cnode=vocindx[u.i16].in;
        while(cnode>=0&&cnode!=symbol&&(uint16_t)(cnode+lenght)!=symbol){
          if(vocbuf[(uint16_t)(symbol+lenght)]==vocbuf[(uint16_t)(cnode+lenght)]){
            i=2;
            uint16_t j=symbol+i,k=cnode+i;
            while(vocbuf[j++]==vocbuf[k]&&k++!=symbol&&i<buf_size) i++;
            if(i>=lenght){
              lenght=i;
              offset=cnode;
            };
            if(lenght==buf_size) break;
          };
          cnode=vocarea[cnode];
        };
      };
      if(lenght>LZ_MIN_MATCH){
        *cpos++=lenght-LZ_MIN_MATCH-1;
        *(uint16_t*)cpos++=(uint16_t)(offset-vocroot-LZ_BUF_SIZE+buf_size);
        buf_size-=lenght;
      }
      else{
        *cbuffer|=0x01;
        *cpos=vocbuf[symbol];
        buf_size--;
      };
    }
    else{
      *cpos++;
      *(uint16_t*)cpos++=0xffff;
      if(eoff) eofs=1;
    };
    cpos++;
    flags--;
    if(!flags||eofs){
      *cbuffer<<=flags;
      if(rc32_write(cbuffer,cpos-cbuffer,ofile)<0) break;
      flags=8;
      cpos=&cbuffer[1];
      if(eofs){
        for(i=sizeof(uint32_t);i;i--){
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
  uint8_t *cpos=cbuffer;
  if(!hlp){
    for(i=0;i<sizeof(uint32_t);i++){
      hlp<<=8;
      *hlpp=fgetc(ifile);
      if(ferror(ifile)||feof(ifile)) return;
    }
  };
  while(1){
    if(!flags){
      cpos=cbuffer;
      if(rc32_read(cpos++,1,ifile)<0) break;
      flags=8;
      lenght=0;
      symbol=*cbuffer;
      for(i=0;i<flags;i++){
        if(symbol&0x80) lenght++;
        else lenght+=3;
        symbol<<=1;
      };
      if(rc32_read(cpos,lenght,ifile)<0) break;
    };
    if(*cbuffer&0x80){
      fputc(*cpos,ofile);
      if(ferror(ofile)) break;
      vocbuf[vocroot++]=*cpos;
    }
    else{
      lenght=*cpos++;
      lenght+=LZ_MIN_MATCH+1;
      offset=*(uint16_t*)cpos++;
      if(offset==0xffff) break;
      offset+=(uint16_t)(vocroot+LZ_BUF_SIZE);
      for(i=0;i<lenght;i++){
        symbol=vocbuf[offset++];
        fputc((uint8_t)symbol,ofile);
        if(ferror(ofile)) return;
        vocbuf[vocroot++]=symbol;
      };
    };
    *cbuffer<<=1;
    cpos++;
    flags--;
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
