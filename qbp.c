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
uint16_t frequency[256][256];
uint16_t fcs[256];
uint16_t buf_size;
uint16_t voclast;
uint16_t vocroot;
uint16_t offset;
uint16_t length;
uint16_t symbol;
uint32_t low;
uint32_t hlp;
uint32_t range;
uint8_t *lowp;
uint8_t *hlpp;
uint8_t cstate;

void pack_initialize(){
  buf_size=flags=vocroot=*cbuffer=low=hlp=icbuf=wpos=rpos=cstate=0;
  voclast=0xfffd;
  range=0xffffffff;
  lowp=&((uint8_t *)&low)[3];
  hlpp=&((uint8_t *)&hlp)[0];
  uint32_t i,j;
  for(i=0;i<256;i++){
    for(j=0;j<256;j++) frequency[i][j]=1;
    fcs[i]=256;
  };
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

inline void rbuf(uint8_t *c,FILE *ifile){
  if(rpos<icbuf) *c=ibuf[rpos++];
  else{
    rpos=0;
    if((icbuf=fread(ibuf,1,0x10000,ifile))) *c=ibuf[rpos++];
  };
}

uint32_t rc32_getc(uint8_t *c,FILE *ifile){
  uint16_t *f=frequency[cstate],fc=fcs[cstate];
  uint32_t count,s=0;
  while((low^(low+range))<0x1000000||range<0x10000||hlp<low){
    hlp<<=8;
    rbuf(hlpp,ifile);
    if(rpos==0) return 0;
    low<<=8;
    range<<=8;
    if((uint32_t)(range+low)<low) range=~low;
  };
  if((count=(hlp-low)/(range/=fc))>=fc) return 1;
  while((s+=*f++)<=count);
  *c=(uint8_t)(--f-frequency[cstate]);
  low+=(s-*f)*range;
  range*=(*f)++;
  if(++fc==0){
    f=frequency[cstate];
    for(s=0;s<256;s++){
      *f=((*f)>>1)|1;
      fc+=*f++;
    };
  };
  fcs[cstate]=fc;
  cstate=*c;
  return 0;
}

uint32_t rc32_putc(uint8_t c,FILE *ofile){
  uint16_t *f=frequency[cstate],fc=fcs[cstate];
  uint32_t s=0,i;
  while((low^(low+range))<0x1000000||range<0x10000){
    wbuf(*lowp,ofile);
    if(wpos==0) return 1;
    low<<=8;
    range<<=8;
    if((uint32_t)(range+low)<low) range=~low;
  };
  range/=fc;
  for(i=0;i<c;i++) s+=f[i];
  low+=s*range;
  range*=f[i]++;
  if(++fc==0){
    for(i=0;i<256;i++){
      *f=((*f)>>1)|1;
      fc+=*f++;
    };
  };
  fcs[cstate]=fc;
  cstate=c;
  return 0;
}

inline void hash(uint16_t s){
  uint16_t h=0;
  for(uint8_t i=0;i<4;i++){
    h^=vocbuf[s++];
    h=(h<<4)^(h>>12);
  };
  hashes[voclast]=h;
}

void pack_file(FILE *ifile,FILE *ofile){
  uint16_t i,rle,rle_shift=0,cnode;
  uint8_t *cpos=&cbuffer[1],*w;
  char eoff=0,eofs=0;
  vocpntr *indx;
  flags=8;
  for(;;){
    if(!eoff){
      if(LZ_BUF_SIZE-buf_size){
        rbuf(&vocbuf[vocroot],ifile);
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
    rle=symbol=vocroot-buf_size;
    if(buf_size){
      cnode=vocbuf[symbol];
      while(rle!=vocroot&&vocbuf[++rle]==cnode);
      rle-=symbol;
      length=LZ_MIN_MATCH;
      if(buf_size>LZ_MIN_MATCH&&rle<buf_size){
        cnode=vocindx[hashes[symbol]].in;
        rle_shift=(uint16_t)(vocroot+LZ_BUF_SIZE-buf_size);
        while(cnode!=symbol){
          if(vocbuf[(uint16_t)(symbol+length)]==vocbuf[(uint16_t)(cnode+length)]){
            uint16_t k=cnode;
            i=symbol;
            while(vocbuf[i++]==vocbuf[k]&&k++!=symbol);
            if((i=(uint16_t)(i-symbol)-1)>=length){
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
          *(uint16_t*)cpos++=~(uint16_t)(offset-rle_shift);
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
        for(i=4;i;i--){
          wbuf(*lowp,ofile);
          if(wpos==0) return;
          low<<=8;
        };
        fwrite(obuf,1,wpos,ofile);
        break;
      };
    };
  };
  return;
}

void unpack_file(FILE *ifile, FILE *ofile){
  uint8_t *cpos=NULL,c,rle_flag=0,bytes=0;
  for(c=0;c<4;c++){
    hlp<<=8;
    rbuf(hlpp,ifile);
    if(rpos==0) return;
  }
  for(;;){
    if(length){
      if(rle_flag==0) c=vocbuf[offset++];
      vocbuf[vocroot++]=c;
      length--;
      bytes=1;
      if(vocroot==0){
        bytes=0;
        if(fwrite(vocbuf,1,0x10000,ofile)<0x10000) break;
      };
    }
    else{
      if(flags==0){
        cpos=cbuffer;
        if(rc32_getc(cpos++,ifile)) break;
        for(c=~*cbuffer;c;flags++) c&=c-1;
        for(c=8+(flags<<1);c;c--)
          if(rc32_getc(cpos++,ifile)) return;
        flags=8;
        cpos=cbuffer+1;
      };
      length=rle_flag=1;
      if(*cbuffer&0x80) c=*cpos;
      else{
        length=LZ_MIN_MATCH+1+*cpos++;
        if((offset=*(uint16_t*)cpos++)<0x0100) c=(uint8_t)(offset);
        else{
          if(offset==0x0100) break;
          offset=~offset+(uint16_t)(vocroot+LZ_BUF_SIZE);
          rle_flag=0;
        };
      };
      *cbuffer<<=1;
      cpos++;
      flags--;
    };
  };
  if(bytes){
    if(vocroot) fwrite(vocbuf,1,vocroot,ofile);
    else fwrite(vocbuf,1,0x10000,ofile);
  };
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
