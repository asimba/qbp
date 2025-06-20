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
#define LZ_MIN_MATCH 3

uint8_t ibuf[0x10000];
uint8_t vocbuf[0x10000];
uint32_t icbuf;
uint32_t rpos;
uint16_t frequency[256][256];
uint16_t fcs[256];
uint32_t low;
uint32_t hlp;
uint32_t range;
uint8_t *lowp;
uint8_t *hlpp;

void rbuf(uint8_t *c,FILE *ifile){
  if(rpos==icbuf&&!(rpos=0,icbuf=fread(ibuf,1,0x10000,ifile))) return;
  *c=ibuf[rpos++];
}

int rc32_getc(uint8_t *c,FILE *ifile,const uint8_t cntx){
  uint16_t fc=fcs[cntx];
  while(hlp<low||(low^(low+range))<0x1000000||range<fc){
    hlp<<=8;
    if(!(rbuf(hlpp,ifile),rpos)) return 0;
    low<<=8;
    range<<=8;
    if(range>~low) range=~low;
  };
  register uint32_t s=0,i;
  if((i=(hlp-low)/(range/=fc))>=fc) return 1;
  uint16_t *f=frequency[cntx];
  while((s+=*f)<=i) f++;
  *c=f-frequency[cntx];
  low+=(s-*f)*range;
  range*=(*f)++;
  if(!++fc){
    f=frequency[cntx];
    for(s=0;s<256;s++) fc+=(*f=((*f)>>1)|(*f&1)),f++;
  };
  fcs[cntx]=fc;
  return 0;
}

void unpack_file(FILE *ifile, FILE *ofile){
  uint8_t cbuffer[3],rle_flag=0,cflags=0,flags=0;
  uint16_t vocroot=0,length=0,offset=0;
  low=hlp=icbuf=rpos=0;
  range=0xffffffff;
  lowp=&((uint8_t *)&low)[3];
  hlpp=&((uint8_t *)&hlp)[0];
  for(int i=0;i<256;i++){
    for(int j=0;j<256;j++) frequency[i][j]=1;
    fcs[i]=256;
  };
  for(int i=0;i<0x10000;i++) vocbuf[i]=0xff;
  for(int i=0;i<4;i++){
    hlp<<=8;
    if(!(rbuf(hlpp,ifile),rpos)) return;
  };
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
        if(rc32_getc((uint8_t *)&offset,ifile,vocbuf[(uint16_t)(vocroot-1)])) return;
      }
      else{
        for(uint8_t c=1;c<4;c++)
          if(rc32_getc(cbuffer+c-1,ifile,c)) return;
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
    if(rc32_getc(&cflags,ifile,0)) break;
    flags=0xff;
  };
  if(length) fwrite(vocbuf,1,vocroot?vocroot:0x10000,ofile);
}

/***********************************************************************************************************/

int main(int argc, char *argv[]){
  if(argc<3){
    printf("qbp file decompressor\n\nto decompress use: unqbp input output\n");
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
