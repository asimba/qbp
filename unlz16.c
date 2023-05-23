#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#ifndef _WIN32
  #include <unistd.h>
#endif

/***********************************************************************************************************/
//Базовая реализация распаковки файла по упрощённому алгоритму LZSS+RLE
/***********************************************************************************************************/

#define LZ_BUF_SIZE 259
#define LZ_CAPACITY 24
#define LZ_MIN_MATCH 3

uint8_t ibuf[0x10000];
uint8_t vocbuf[0x10000];
uint32_t icbuf;
uint32_t rpos;

inline void rbuf(uint8_t *c,FILE *ifile){
  if(rpos==icbuf&&!(rpos=0,icbuf=fread(ibuf,1,0x10000,ifile))) return;
  *c=ibuf[rpos++];
}

void unpack_file(FILE *ifile, FILE *ofile){
  uint16_t vocroot=0,offset=0,length=0;
  uint8_t flags=0,rle_flag=0,bytes=0,c,cbuffer[LZ_CAPACITY+1],*cpos=cbuffer;
  for(int i=0;i<0x10000;i++) vocbuf[i]=0xff;
  icbuf=rpos=0;
  for(;;){
    if(length){
      if(!rle_flag) c=vocbuf[offset++];
      vocbuf[vocroot++]=c;
      length--;
      bytes=1;
      if(!vocroot&&(bytes=0,fwrite(vocbuf,1,0x10000,ofile)<0x10000)) break;
      continue;
    };
    if(flags){
      length=rle_flag=1;
      if(*cbuffer&0x80) c=*cpos;
      else{
        length+=LZ_MIN_MATCH+*cpos++;
        if((offset=*(uint16_t*)cpos++)<0x0100) c=offset;
        else{
          if(offset==0x0100) break;
          offset=~offset+vocroot+LZ_BUF_SIZE;
          rle_flag=0;
        };
      };
      *cbuffer<<=1;
      cpos++;
      flags--;
      continue;
    };
    cpos=cbuffer;
    if(!(rbuf(cpos++,ifile),rpos)) break;
    for(c=~*cbuffer;c;flags++) c&=c-1;
    for(c=8+(flags<<1);c;c--)
      if(!(rbuf(cpos++,ifile),rpos)) break;
    cpos=&cbuffer[1];
    flags=8;
  };
  if(bytes) fwrite(vocbuf,1,vocroot?vocroot:0x10000,ofile);
}

/***********************************************************************************************************/

int main(int argc, char *argv[]){
  if(argc<3){
    printf("unlz16 file decompressor\n\nto decompress use: unlz16 input output\n");
    goto rpoint00;
  };
  if(access(argv[2],0)==0){
    printf("Error: output file already exists!\n");
    goto rpoint00;
  };
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
