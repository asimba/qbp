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

typedef struct{
  uint16_t in;
  uint16_t out;
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
uint16_t lenght;
uint16_t symbol;

void pack_initialize(){
  buf_size=flags=vocroot=*cbuffer=icbuf=wpos=rpos=0;
  voclast=0xfffd;
  uint32_t i;
  for(i=0;i<0x10000;i++){
    vocbuf[i]=0xff;
    hashes[i]=0xc0c0;
    vocindx[i].in=1;
    vocindx[i].out=0;
    vocarea[i]=(uint16_t)(i+1);
  };
  vocindx[0xc0c0].in=0;
  vocindx[0xc0c0].out=0xfffc;
  vocarea[0xfffc]=0xfffc;
  vocarea[0xfffd]=0xfffd;
  vocarea[0xfffe]=0xfffe;
  vocarea[0xffff]=0xffff;
}

inline uint8_t wbuf(uint8_t c,FILE *ofile){
  if(wpos<0x10000) obuf[wpos++]=c;
  else{
    if(fwrite(obuf,1,wpos,ofile)<wpos) return 1;
    *obuf=c;
    wpos=1;
  };
  return 0;
}

inline uint8_t rbuf(uint8_t *c,FILE *ifile){
  if(icbuf--) *c=ibuf[rpos++];
  else{
    icbuf=fread(ibuf,1,0x10000,ifile);
    if(ferror(ifile)) return 1;
    if(icbuf){
      *c=*ibuf;
      rpos=1;
      icbuf--;
    }
    else rpos=0;
  };
  return 0;
}

void pack_file(FILE *ifile,FILE *ofile){
  uint16_t i,rle,rle_shift,cnode,h;
  uint8_t *cpos=&cbuffer[1],*w,c;
  char eoff=0,eofs=0;
  vocpntr *indx;
  flags=8;
  for(;;){
    if(!eoff){
      if(LZ_BUF_SIZE-buf_size){
        if(rbuf(&c,ifile)) break;
        if(rpos==0){
          eoff=1;
          continue;
        }
        else{
          h=hashes[vocroot];
          if(vocarea[vocroot]==vocroot){
            vocindx[h].in=1;
            vocindx[h].out=0;
          }
          else vocindx[h].in=vocarea[vocroot];
          vocarea[vocroot]=vocroot;
          vocbuf[vocroot]=c;
          h=(uint16_t)vocbuf[voclast];
          h<<=4;
          h^=(uint16_t)vocbuf[(uint16_t)(voclast+1)];
          h<<=4;
          h^=(uint16_t)vocbuf[(uint16_t)(voclast+2)];
          h=(h<<2)^(h>>14);
          h^=(uint16_t)vocbuf[vocroot];
          hashes[voclast]=h;
          indx=&vocindx[h];
          if(indx->in==1&&indx->out==0) indx->in=voclast;
          else vocarea[indx->out]=voclast;
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
      rle=1;
      while(rle<buf_size){
        if(vocbuf[symbol]==vocbuf[(uint16_t)(symbol+rle)]) rle++;
        else break;
      };
      lenght=LZ_MIN_MATCH;
      if(buf_size>LZ_MIN_MATCH){
        cnode=vocindx[hashes[symbol]].in;
        rle_shift=(uint16_t)(vocroot+LZ_BUF_SIZE-buf_size);
        while(cnode!=symbol){
          if(vocbuf[(uint16_t)(symbol+lenght)]==vocbuf[(uint16_t)(cnode+lenght)]){
            i=0;
            uint16_t j=symbol+i,k=cnode+i;
            while(vocbuf[j++]==vocbuf[k]&&k++!=symbol) i++;
            if(i>=lenght){
              if((uint16_t)(cnode-rle_shift)>=0xfeff) break;
              if(i>buf_size) lenght=buf_size;
              else lenght=i;
              offset=cnode;
            };
          };
          cnode=vocarea[cnode];
        };
      };
      if(rle>lenght){
        *cpos++=rle-LZ_MIN_MATCH-1;
        *(uint16_t*)cpos++=((uint16_t)(vocbuf[symbol]))+0xfeff;
        buf_size-=rle;
      }
      else{
        if(lenght>LZ_MIN_MATCH){
          *cpos++=lenght-LZ_MIN_MATCH-1;
          *(uint16_t*)cpos++=(uint16_t)(offset-rle_shift);
          buf_size-=lenght;
        }
        else{
          *cbuffer|=1;
          *cpos=vocbuf[symbol];
          buf_size--;
        };
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
      w=cbuffer;
      for(i=cpos-cbuffer;i;i--)
        if(wbuf(*w++,ofile)) return;
      flags=8;
      cpos=&cbuffer[1];
      if(eofs) break;
    };
  };
  fwrite(obuf,1,wpos,ofile);
  return;
}

void unpack_file(FILE *ifile, FILE *ofile){
  uint16_t i;
  uint8_t *cpos=cbuffer,c;
  for(;;){
    if(!flags){
      cpos=cbuffer;
      if(rbuf(cpos++,ifile)) return;
      flags=8;
      lenght=0;
      c=*cbuffer;
      for(i=0;i<flags;i++){
        if(c&0x80) lenght++;
        else lenght+=3;
        c<<=1;
      };
      for(i=lenght;i;i--)
        if(rbuf(cpos++,ifile)) return;
      cpos=cbuffer+1;
    };
    if(*cbuffer&0x80){
      if(wbuf(*cpos,ofile)) break;
      vocbuf[vocroot++]=*cpos;
    }
    else{
      lenght=*cpos++;
      lenght+=LZ_MIN_MATCH+1;
      offset=*(uint16_t*)cpos++;
      if(offset==0xffff) break;
      if(offset>0xfefe){
        c=(uint8_t)(offset-0xfeff);
        for(i=0;i<lenght;i++){
          if(wbuf(c,ofile)) return;
          vocbuf[vocroot++]=c;
        };
      }
      else{
        offset+=(uint16_t)(vocroot+LZ_BUF_SIZE);
        for(i=0;i<lenght;i++){
          c=vocbuf[offset++];
          if(wbuf(c,ofile)) return;
          vocbuf[vocroot++]=c;
        };
      };
    };
    *cbuffer<<=1;
    cpos++;
    flags--;
  };
  fwrite(obuf,1,wpos,ofile);
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
