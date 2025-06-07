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
  uint8_t flags=0,cflags=0,rle_flag=0,c,cbuffer[3];
  for(int i=0;i<0x10000;i++) vocbuf[i]=0xff;
  cbuffer[0]=icbuf=rpos=0;
  for(;;){
    if(length){
      if(rle_flag) vocbuf[vocroot++]=offset;
      else vocbuf[vocroot++]=vocbuf[offset++];
      length--;
      if(!vocroot&&(fwrite(vocbuf,1,0x10000,ofile)<0x10000)) break;
      continue;
    };
    if(flags){
      length=rle_flag=1;
      if(cflags&0x80){
        if(!(rbuf((uint8_t *)&offset,ifile),rpos)) break;
      }
      else{
        for(c=0;c<3;c++)
          if(!(rbuf(cbuffer+c,ifile),rpos)) return;
        length=LZ_MIN_MATCH+1+*cbuffer;
        if((offset=*(uint16_t*)(cbuffer+1))>=0x0100){
          if(offset==0x0100) break;
          offset=~offset+vocroot+LZ_BUF_SIZE;
          rle_flag=0;
        };
      };
      cflags<<=1;
      flags<<=1;
      continue;
    };
    if(!(rbuf(&cflags,ifile),rpos)) break;
    flags=0xff;
  };
  if(length) fwrite(vocbuf,1,vocroot?vocroot:0x10000,ofile);
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
