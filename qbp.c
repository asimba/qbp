#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#ifndef _WIN32
  #include <unistd.h>
#endif

/***********************************************************************************************************/
//Базовая реализация упаковки/распаковки файла по упрощённому алгоритму LZSS+RLE+RC32
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
uint8_t *wpntr;
uint32_t icbuf;
uint32_t wpos;
uint32_t rpos;
uint8_t flags;
uint8_t cbuffer[LZ_CAPACITY+1];
uint8_t vocbuf[0x10000];
uint16_t vocarea[0x10000];
uint16_t hashes[0x10000];
vocpntr vocindx[0x10000];
uint16_t frequency[256];
uint16_t buf_size;
uint16_t voclast;
uint16_t vocroot;
uint16_t offset;
uint16_t length;
uint16_t symbol;
uint32_t low;
uint32_t hlp;
uint32_t range;
uint16_t fc;
uint8_t *lowp;
uint8_t *hlpp;

void pack_initialize(){
  buf_size=flags=vocroot=*cbuffer=low=hlp=icbuf=wpos=rpos=0;
  voclast=0xfffd;
  range=0xffffffff;
  lowp=&((uint8_t *)&low)[3];
  hlpp=&((uint8_t *)&hlp)[0];
  uint32_t i;
  for(i=0;i<256;i++) frequency[i]=1;
  fc=256;
  for(i=0;i<0x10000;i++){
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
  wpntr=obuf;
}

inline void wbuf(uint8_t c,FILE *ofile){
  if(wpos<0x10000){
    *wpntr++=c;
    wpos++;
  }
  else{
    if(fwrite(obuf,1,wpos,ofile)==wpos){
      *obuf=c;
      wpos=1;
      wpntr=obuf+1;
      return;
    };
    wpos=0;
  };
}

inline uint32_t rbuf(uint8_t *c,FILE *ifile){
  if(rpos<icbuf) *c=ibuf[rpos++];
  else{
    rpos=0;
    icbuf=fread(ibuf,1,0x10000,ifile);
    if(ferror(ifile)) return 1;
    if(icbuf) *c=ibuf[rpos++];
  };
  return 0;
}

inline void rc32_rescale(uint32_t s){
  low+=s*range;
  range*=frequency[symbol]++;
  if(++fc==0){
    for(uint16_t i=0;i<256;i++){
      if((frequency[i]>>=1)==0) frequency[i]=1;
      fc+=frequency[i];
    };
  };
}

uint32_t rc32_getc(uint8_t *c,FILE *ifile){
  while((range<0x10000)||(hlp<low)){
    hlp<<=8;
    if(rbuf(hlpp,ifile)) return 1;
    if(rpos==0) return 0;
    low<<=8;
    range<<=8;
    if((uint32_t)(range+low)<low) range=0xffffffff-low;
  };
  range/=fc;
  uint32_t count=(hlp-low)/range,s=0;
  if(count>=fc) return 1;
  for(symbol=0;symbol<256;symbol++){
    s+=frequency[symbol];
    if(s>count) break;
  };
  *c=(uint8_t)symbol;
  s-=frequency[symbol];
  rc32_rescale(s);
  return 0;
}

uint32_t rc32_putc(uint8_t c,FILE *ofile){
  while(range<0x10000){
    wbuf(*lowp,ofile);
    if(wpos==0) return 1;
    low<<=8;
    range<<=8;
    if((uint32_t)(range+low)<low) range=0xffffffff-low;
  };
  symbol=c;
  range/=fc;
  uint32_t s=0;
  for(uint16_t i=0;i<symbol;i++) s+=frequency[i];
  rc32_rescale(s);
  return 0;
}

inline void hash(uint16_t s){
  uint16_t h=0;
  for(uint8_t i=0;i<sizeof(uint32_t);i++){
    h^=vocbuf[s++];
    h=(h<<4)^(h>>12);
  };
  hashes[voclast]=h;
}

void pack_file(FILE *ifile,FILE *ofile){
  uint16_t i,rle,rle_shift,cnode;
  uint8_t *cpos=&cbuffer[1],*w;
  char eoff=0,eofs=0;
  vocpntr *indx;
  flags=8;
  for(;;){
    if(!eoff){
      if(LZ_BUF_SIZE-buf_size){
        if(rbuf(&vocbuf[vocroot],ifile)) break;
        if(rpos==0){
          eoff=1;
          continue;
        }
        else{
          if(vocarea[vocroot]==vocroot) vocindx[hashes[vocroot]].val=1;
          else vocindx[hashes[vocroot]].in=vocarea[vocroot];
          vocarea[vocroot]=vocroot;
          hash(voclast);
          indx=&vocindx[hashes[voclast]];
          if(indx->val==1) indx->in=voclast;
          else vocarea[indx->out]=voclast;
          indx->out=voclast;
          voclast++;
          vocroot++;
          buf_size++;
          continue;
        };
      };
    }
    *cbuffer<<=1;
    symbol=vocroot-buf_size;
    if(buf_size){
      rle=1;
      while(rle<buf_size){
        if(vocbuf[symbol]==vocbuf[(uint16_t)(symbol+rle)]) rle++;
        else break;
      };
      length=LZ_MIN_MATCH;
      if(buf_size>LZ_MIN_MATCH){
        cnode=vocindx[hashes[symbol]].in;
        rle_shift=(uint16_t)(vocroot+LZ_BUF_SIZE-buf_size);
        while(cnode!=symbol){
          if(vocbuf[(uint16_t)(symbol+length)]==vocbuf[(uint16_t)(cnode+length)]){
            i=0;
            uint16_t j=symbol,k=cnode;
            while(vocbuf[j++]==vocbuf[k]&&k++!=symbol) i++;
            if(i>=length){
              //while buf_size==LZ_BUF_SIZE: minimal offset > 0x0104;
              if(buf_size<LZ_BUF_SIZE){
                if((uint16_t)(cnode-rle_shift)>0xfeff){
                  cnode=vocarea[cnode];
                  continue;
                };
              };
              offset=cnode;
              if(i>=buf_size){
                length=buf_size;
                break;
              }
              else length=i;
            };
          };
          cnode=vocarea[cnode];
        };
      };
      if(rle>length){
        *cpos++=rle-LZ_MIN_MATCH-1;
        *(uint16_t*)cpos++=((uint16_t)(vocbuf[symbol]));
        buf_size-=rle;
      }
      else{
        if(length>LZ_MIN_MATCH){
          *cpos++=length-LZ_MIN_MATCH-1;
          *(uint16_t*)cpos++=0xffff-(uint16_t)(offset-rle_shift);
          buf_size-=length;
        }
        else{
          *cbuffer|=0x01;
          *cpos=vocbuf[symbol];
          buf_size--;
        };
      };
    }
    else{
      cpos++;
      *(uint16_t*)cpos++=0x0100;
      if(eoff) eofs=1;
    };
    cpos++;
    flags--;
    if(!flags||eofs){
      *cbuffer<<=flags;
      w=cbuffer;
      for(i=cpos-cbuffer;i;i--)
        if(rc32_putc(*w++,ofile)) return;
      flags=8;
      cpos=&cbuffer[1];
      if(eofs){
        for(i=sizeof(uint32_t);i;i--){
          wbuf(*lowp,ofile);
          if(wpos==0) return;
          low<<=8;
        }
        fwrite(obuf,1,wpos,ofile);
        break;
      };
    };
  };
  return;
}

void unpack_file(FILE *ifile, FILE *ofile){
  uint8_t *cpos,c;
  for(offset=0;offset<sizeof(uint32_t);offset++){
    hlp<<=8;
    if(rbuf(hlpp,ifile)||rpos==0) return;
  }
  for(;;){
    if(!flags){
      cpos=cbuffer;
      if(rc32_getc(cpos++,ifile)) return;
      flags=8;
      length=0;
      c=*cbuffer;
      for(int i=8;i;i--){
        if(c&0x80) length++;
        else length+=3;
        c<<=1;
      };
      for(int i=length;i;i--)
        if(rc32_getc(cpos++,ifile)) return;
      cpos=cbuffer+1;
    };
    if(*cbuffer&0x80){
      vocbuf[vocroot++]=*cpos;
      wbuf(*cpos,ofile);
      if(wpos==0) break;
    }
    else{
      length=LZ_MIN_MATCH+1+*cpos++;
      if((offset=*(uint16_t*)cpos++)<0x0100){
        c=(uint8_t)(offset);
        for(int i=length;i;i--){
          vocbuf[vocroot++]=c;
          wbuf(c,ofile);
          if(wpos==0) return;
        };
      }
      else{
        if(offset==0x0100) break;
        offset=0xffff-offset+(uint16_t)(vocroot+LZ_BUF_SIZE);
        for(int i=length;i;i--){
          c=vocbuf[offset++];
          vocbuf[vocroot++]=c;
          wbuf(c,ofile);
          if(wpos==0) return;
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
  if((argc<4)||(access(argv[3],0)==0)||\
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
