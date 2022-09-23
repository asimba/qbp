#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#ifndef _WIN32
  #include <unistd.h>
#endif

/***********************************************************************************************************/
//Базовая реализация распаковки файла по упрощённому алгоритму LZSS+RLE+RC32
/***********************************************************************************************************/

#define LZ_BUF_SIZE 259
#define LZ_CAPACITY 24
#define LZ_MIN_MATCH 3

uint8_t ibuf[0x10000];
uint8_t vocbuf[0x10000];
uint32_t icbuf;
uint32_t rpos;
uint16_t frequency[256];
uint32_t low;
uint32_t hlp;
uint32_t range;
uint16_t fc;
uint8_t *hlpp;

inline void rbuf(uint8_t *c,FILE *ifile){
  if(rpos<icbuf) *c=ibuf[rpos++];
  else{
    rpos=0;
    if(icbuf=fread(ibuf,1,0x10000,ifile)) *c=ibuf[rpos++];
  };
}

uint32_t rc32_getc(uint8_t *c,FILE *ifile){
  while((range<0x10000)||(hlp<low)){
    hlp<<=8;
    rbuf(hlpp,ifile);
    if(rpos==0) return 0;
    low<<=8;
    range<<=8;
    if((uint32_t)(range+low)<low) range=0xffffffff-low;
  };
  range/=fc;
  uint32_t count=(hlp-low)/range,s=0;
  if(count>=fc) return 1;
  for(int i=0;i<256;i++){
    s+=frequency[i];
    if(s>count){
      *c=(uint8_t)i;
      break;
    };
  };
  low+=(s-frequency[*c])*range;
  range*=frequency[*c]++;
  if(++fc==0){
    for(int i=0;i<256;i++){
      if((frequency[i]>>=1)==0) frequency[i]=1;
      fc+=frequency[i];
    };
  };
  return 0;
}

void unpack_file(FILE *ifile, FILE *ofile){
  uint8_t *cpos,flags=0,c,rle_flag=0;
  uint8_t cbuffer[LZ_CAPACITY+1];
  uint16_t vocroot=0,offset=0,length=0;
  low=hlp=icbuf=rpos=0;
  range=0xffffffff;
  hlpp=&((uint8_t *)&hlp)[0];
  for(int i=0;i<256;i++) frequency[i]=1;
  fc=256;
  for(int i=0;i<0x10000;i++) vocbuf[i]=0xff;
  for(c=0;c<sizeof(uint32_t);c++){
    hlp<<=8;
    rbuf(hlpp,ifile);
    if(rpos==0) return;
  }
  for(;;){
    if(length){
      if(rle_flag==0) c=vocbuf[offset++];
      vocbuf[vocroot++]=c;
      length--;
    }
    else{
      if(flags==0){
        cpos=cbuffer;
        if(rc32_getc(cpos++,ifile)) break;
        c=*cbuffer;
        length=8;
        for(flags=0;flags<8;flags++){
          if((c&0x1)==0) length+=2;
          c>>=1;
        };
        for(c=length;c;c--)
          if(rc32_getc(cpos++,ifile)) return;
        cpos=cbuffer+1;
      };
      if(*cbuffer&0x80){
        length=0;
        vocbuf[vocroot++]=*cpos;
      }
      else{
        length=LZ_MIN_MATCH+1+*cpos++;
        if((offset=*(uint16_t*)cpos++)<0x0100){
          c=(uint8_t)(offset);
          rle_flag=1;
        }
        else{
          if(offset==0x0100) break;
          offset=0xffff-offset+(uint16_t)(vocroot+LZ_BUF_SIZE);
          c=vocbuf[offset++];
          rle_flag=0;
        };
        vocbuf[vocroot++]=c;
        length--;
      };
      *cbuffer<<=1;
      cpos++;
      flags--;
    };
    if(vocroot==0)
      if(fwrite(vocbuf,1,0x10000,ofile)<0x10000) break;
  };
  if(length){
    if(vocroot) fwrite(vocbuf,1,vocroot,ofile);
    else fwrite(vocbuf,1,0x10000,ofile);
  };
  return;
}

/***********************************************************************************************************/

int main(int argc, char *argv[]){
  if((argc<3)||(access(argv[2],0)==0)) goto rpoint00;
  FILE *ifile,*ofile;
  if(!(ifile=fopen(argv[1],"rb"))) goto rpoint00;
  if(!(ofile=fopen(argv[2],"wb"))) goto rpoint01;
  unpack_file(ifile,ofile);
  fclose(ofile);
rpoint01:
  fclose(ifile);
rpoint00:
  return 0;
}
