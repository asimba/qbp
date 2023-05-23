#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#ifndef _WIN32
  #include <unistd.h>
#endif

/***********************************************************************************************************/
//Базовая реализация упаковки/распаковки файла по упрощённому алгоритму LZSS+RLE
/***********************************************************************************************************/

#define LZ_BUF_SIZE 259
#define LZ_CAPACITY 24
#define LZ_MIN_MATCH 3

typedef union{
  struct{
    uint16_t in;
    uint16_t out;
  };
  uint32_t val;
} vocpntr;

uint8_t ibuf[0x10000];
uint8_t obuf[0x10000];
uint32_t icbuf;
uint32_t wpos;
uint32_t rpos;
uint8_t flags;
uint8_t cbuffer[LZ_CAPACITY+1];
uint8_t vocbuf[0x10000];
uint16_t vocarea[0x10000];
uint16_t hashes[0x10000];
vocpntr vocindx[0x10000];
uint16_t buf_size;
uint16_t voclast;
uint16_t vocroot;
uint16_t offset;
uint16_t length;

void pack_initialize(){
  flags=buf_size=vocroot=icbuf=wpos=rpos=0;
  voclast=0xfffc;
  for(int i=0;i<0x10000;i++){
    vocbuf[i]=0xff;
    hashes[i]=0;
    vocindx[i].val=1;
    vocarea[i]=(uint16_t)(i+1);
  };
  vocindx[0].in=0;
  vocindx[0].out=0xfffc;
  vocarea[0xfffc]=0xfffc;
  vocarea[0xfffd]=0xfffd;
  vocarea[0xfffe]=0xfffe;
  vocarea[0xffff]=0xffff;
}

void wbuf(uint8_t c,FILE *ofile){
  if(wpos==0x10000&&(wpos=0,fwrite(obuf,1,0x10000,ofile)!=0x10000)) return;
  obuf[wpos++]=c;
}

void rbuf(uint8_t *c,FILE *ifile){
  if(rpos==icbuf&&!(rpos=0,icbuf=fread(ibuf,1,0x10000,ifile))) return;
  *c=ibuf[rpos++];
}

void pack_file(FILE *ifile,FILE *ofile){
  uint8_t *cpos=&cbuffer[1],eoff=0;
  flags=8;
  for(;;){
    if(!eoff&&buf_size!=LZ_BUF_SIZE){
      if((rbuf(&vocbuf[vocroot],ifile),rpos)){
        if(vocarea[vocroot]==vocroot) vocindx[hashes[vocroot]].val=1;
        else vocindx[hashes[vocroot]].in=vocarea[vocroot];
        vocarea[vocroot]=vocroot;
        uint16_t hs=hashes[voclast]^vocbuf[voclast]^vocbuf[vocroot++];
        vocpntr *indx=&vocindx[(hashes[++voclast]=(hs<<4)|(hs>>12))];
        if(indx->val==1) indx->in=voclast;
        else vocarea[indx->out]=voclast;
        indx->out=voclast;
        buf_size++;
      }
      else eoff=1;
      continue;
    };
    *cbuffer<<=1;
    if(buf_size){
      uint16_t symbol=vocroot-buf_size,rle=symbol,rle_shift=symbol+LZ_BUF_SIZE;
      while(rle!=vocroot&&vocbuf[++rle]==vocbuf[symbol]);
      rle-=symbol;
      if(buf_size>(length=LZ_MIN_MATCH)&&rle!=buf_size){
        uint16_t cnode=vocindx[hashes[symbol]].in;
        while(cnode!=symbol){
          if(vocbuf[(uint16_t)(symbol+length)]==vocbuf[(uint16_t)(cnode+length)]){
            uint16_t i=symbol,j=cnode;
            while(i!=vocroot&&vocbuf[i]==vocbuf[j++]) i++;
            if((i-=symbol)>=length){
              if(buf_size!=LZ_BUF_SIZE&&(uint16_t)(cnode-rle_shift)>0xfefe) cnode=vocarea[cnode];
              else{
                offset=cnode;
                if((length=i)==buf_size) break;
              };
            };
          };
          cnode=vocarea[cnode];
        };
      };
      if(rle>length){
        length=rle;
        offset=vocbuf[symbol];
      }
      else offset=~(uint16_t)(offset-rle_shift);
      uint16_t i=length-LZ_MIN_MATCH;
      if(i){
        *cpos++=--i;
        *(uint16_t*)cpos++=offset;
        buf_size-=length;
      }
      else{
        *cbuffer|=1;
        *cpos=vocbuf[symbol];
        buf_size--;
      };
    }
    else{
      length=0;
      cpos++;
      *(uint16_t*)cpos++=0x0100;
    };
    cpos++;
    if(--flags&&length) continue;
    *cbuffer<<=flags;
    for(int i=0;i<cpos-cbuffer;i++)
      if(!(wbuf(cbuffer[i],ofile),wpos)) return;
    if(!length) break;
    cpos=&cbuffer[1];
    flags=8;
  };
  fwrite(obuf,1,wpos,ofile);
}

void unpack_file(FILE *ifile, FILE *ofile){
  uint8_t *cpos=NULL,c,rle_flag=0,bytes=0;
  length=0;
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
  if((argc<4)||((argv[1][0]!='c')&&(argv[1][0]!='d'))){
    printf("lz16 file compressor\n\nto   compress use: lz16 c input output\nto decompress use: lz16 d input output\n");
    goto rpoint00;
  };
  if(access(argv[3],0)==0){
    printf("Error: output file already exists!\n");
    goto rpoint00;
  };
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
