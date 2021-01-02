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

#define LZ_BUF_SIZE 258
#define LZ_CAPACITY 24
#define LZ_MIN_MATCH 3

typedef struct{
  int32_t in;
  int32_t out;
} vocpntr;

uint8_t cbuffer[LZ_CAPACITY+1];
uint8_t lzbuf[LZ_BUF_SIZE+1];
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
char eofs;

void pack_initialize(){
  buf_size=cflags_count=offset=\
    lenght=vocroot=cbuffer[0]=eofs=\
      low=hlp=0;
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
  uint16_t j;
  low+=frequency[symbol]*range;
  range*=frequency[symbol+1]-frequency[symbol];
  for(j=symbol+1; j<257; j++) frequency[j]++;
  if(*fc>0xffff){
    frequency[0]>>=1;
    for(j=1; j<257; j++){
      frequency[j]>>=1;
      if(frequency[j]<=frequency[j-1])
        frequency[j]=frequency[j-1]+1;
    };
  };
}

int rc32_read(uint8_t *buf,int l,FILE *ifile){
  while(l--){
    if(!hlp){
      eofs=1;
      return 1;
    };
    range/=*fc;
    uint32_t count=(hlp-low)/range;
    if(count>=*fc) return -1;
    for(symbol=255; frequency[symbol]>count; symbol--) if(!symbol) break;
    *buf=(uint8_t)symbol;
    rc32_rescale();
    while((range<0x10000)||(hlp<low)){
      if(((low&0xff0000)==0xff0000)&&(range+(uint16_t)low>=0x10000))
        range=0x10000-(uint16_t)low;
      hlp<<=8;
      *hlpp=fgetc(ifile);
      if(ferror(ifile)) return -1;
      if(feof(ifile)){
        hlp=0;
        break;
      }
      low<<=8;
      range<<=8;
    };
    buf++;
  };
  return 1;
}

int rc32_write(uint8_t *buf,int l,FILE *ofile){
  while(l--){
    symbol=*buf;
    range/=*fc;
    rc32_rescale();
    while(range<0x10000){
      if(((low&0xff0000)==0xff0000)&&(range+(uint16_t)low>=0x10000))
        range=0x10000-(uint16_t)low;
      fputc(*lowp,ofile);
      if(ferror(ofile)) return -1;
      low<<=8;
      range<<=8;
    };
    buf++;
  };
  return 1;
}

void pack_file(FILE *ifile,FILE *ofile){
  char eoff=0;
  vocpntr *indx;
  uint8_t *str;
  uint16_t i,tnode,cl;
  int32_t cnode;
  union {uint8_t c[sizeof(uint16_t)];uint16_t i16;} u;
  while(1){
    if(!eoff){
      if(LZ_BUF_SIZE-buf_size){
        lzbuf[buf_size]=fgetc(ifile);
        if(ferror(ifile)) break;
        if(feof(ifile)) eoff=1;
        else{
          buf_size++;
          continue;
        };
      };
    };
    offset=0;
    lenght=1;
    cbuffer[0]<<=1;
    if(buf_size){
      if(buf_size>=LZ_MIN_MATCH){
        cnode=vocindx[*(uint16_t*)lzbuf].in;
        while(cnode>=0){
          if(lzbuf[lenght]==vocbuf[(uint16_t)(cnode+lenght)]){
            tnode=cnode+2;
            cl=2;
            while((cl<buf_size)&&(lzbuf[cl]==vocbuf[tnode++])) cl++;
            if(cl>=lenght&&cl>2){
              lenght=cl;
              offset=cnode;
            };
            if(lenght==buf_size) break;
          };
          cnode=vocarea[cnode];
        };
        if(lenght>1) offset-=vocroot;
      };
      if(lenght>=LZ_MIN_MATCH){
        cbuffer[cbuffer_position++]=(uint8_t)(lenght-LZ_MIN_MATCH);
        *(uint16_t*)&cbuffer[cbuffer_position++]=offset;
      }
      else{
        cbuffer[0]|=0x01;
        cbuffer[cbuffer_position]=*lzbuf;
      };
      str=lzbuf;
      i=lenght;
      while(i--){
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
      memmove(lzbuf,lzbuf+lenght,LZ_BUF_SIZE-lenght);
      buf_size-=lenght;
    }
    else{
      cbuffer[cbuffer_position++]=0;
      *(uint16_t*)&cbuffer[cbuffer_position++]=0xffff;
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
  uint16_t i,c=0;
  if(!hlp){
    for(i=0;i<sizeof(uint32_t);i++){
      hlp<<=8;
      *hlpp=fgetc(ifile);
      if(ferror(ifile)||feof(ifile)){
        hlp=0;
        return;
      };
    }
  };
  while(1){
    if(cflags_count==0){
      if(rc32_read(cbuffer,1,ifile)<0) break;
      if(eofs) break;
      cflags_count=8;
      cbuffer_position=1;
      c=0;
      uint8_t f=cbuffer[0];
      for(uint8_t cf=0;cf<cflags_count;cf++){
        if(f&0x80) c++;
        else c+=3;
        f<<=1;
      };
      if(rc32_read(&cbuffer[1],c,ifile)<0) break;
    };
    if(cbuffer[0]&0x80){
      fputc(cbuffer[cbuffer_position],ofile);
      if(ferror(ofile)) break;
      vocbuf[vocroot++]=cbuffer[cbuffer_position];
    }
    else{
      c=cbuffer[cbuffer_position++]+LZ_MIN_MATCH;
      buf_size=*(uint16_t*)&cbuffer[cbuffer_position++];
      if(buf_size==0xffff) break;
      buf_size+=vocroot;
      for(i=0;i<c;i++){
        lzbuf[i]=vocbuf[buf_size++];
        fputc(lzbuf[i],ofile);
        if(ferror(ofile)) return;
      };
      for(i=0;i<c;i++) vocbuf[vocroot++]=lzbuf[i];
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
