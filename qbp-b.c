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

typedef struct{
  uint16_t in;
  uint16_t out;
} vocpntr;

uint8_t ibuf[0x10000];
uint8_t obuf[0x10000];
int32_t icbuf;
int32_t ocbuf;
uint32_t rpos;
uint8_t flags;
uint8_t cbuffer[LZ_CAPACITY+1];
uint8_t vocbuf[0x10000];
uint16_t vocarea[0x10000];
uint16_t hashes[0x10000];
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
  buf_size=flags=vocroot=*cbuffer=low=hlp=icbuf=ocbuf=rpos=0;
  voclast=0xfffd;
  range=0xffffffff;
  lowp=&((char *)&low)[3];
  hlpp=&((char *)&hlp)[0];
  fc=&frequency[256];
  uint32_t i;
  for(i=0;i<257;i++) frequency[i]=i;
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
      if(icbuf) *hlpp=ibuf[rpos++];
      else{
        icbuf=fread(ibuf,1,0x10000,ifile);
        if(ferror(ifile)) return -1;
        if(icbuf){
          *hlpp=*ibuf;
          rpos=1;
        };
      }
      if(icbuf==0) return 1;
      icbuf--;
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
      if(ocbuf<0x10000) obuf[ocbuf++]=*lowp;
      else{
        if(fwrite(obuf,1,ocbuf,ofile)<ocbuf||ferror(ofile)) return -1;
        *obuf=*lowp;
        ocbuf=1;
      };
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
  uint16_t i,rle,rle_shift,cnode,h;
  char eoff=0,eofs=0;
  vocpntr *indx;
  uint8_t *cpos=&cbuffer[1];
  flags=8;
  for(;;){
    if(!eoff){
      if(LZ_BUF_SIZE-buf_size){
        if(icbuf--){
          h=hashes[vocroot];
          if(vocarea[vocroot]==vocroot){
            vocindx[h].in=1;
            vocindx[h].out=0;
          }
          else vocindx[h].in=vocarea[vocroot];
          vocarea[vocroot]=vocroot;
          vocbuf[vocroot]=ibuf[rpos++];;
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
        }
        else{
          rpos=0;
          icbuf=fread(ibuf,1,0x10000,ifile);
          if(ferror(ifile)) break;
          if(icbuf) continue;
          eoff=1;
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
          *cbuffer|=0x01;
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
      if(rc32_write(cbuffer,cpos-cbuffer,ofile)<0) break;
      flags=8;
      cpos=&cbuffer[1];
      if(eofs){
        if(ocbuf)
          if(fwrite(obuf,1,ocbuf,ofile)<ocbuf||ferror(ofile)) return;
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
      if(ocbuf<0x10000) obuf[ocbuf++]=*cpos;
      else{
        if(fwrite(obuf,1,ocbuf,ofile)<ocbuf||ferror(ofile)) break;
        obuf[0]=*cpos;
        ocbuf=1;
      };
      vocbuf[vocroot++]=*cpos;
    }
    else{
      lenght=*cpos++;
      lenght+=LZ_MIN_MATCH+1;
      offset=*(uint16_t*)cpos++;
      if(offset==0xffff) break;
      if(offset>0xfefe){
        symbol=offset-0xfeff;
        for(i=0;i<lenght;i++){
          if(ocbuf<0x10000) obuf[ocbuf++]=symbol;
          else{
            if(fwrite(obuf,1,ocbuf,ofile)<ocbuf||ferror(ofile)) return;
            obuf[0]=symbol;
            ocbuf=1;
          };
          vocbuf[vocroot++]=(uint8_t)symbol;
        };
      }
      else{
        offset+=(uint16_t)(vocroot+LZ_BUF_SIZE);
        for(i=0;i<lenght;i++){
          symbol=vocbuf[offset++];
          if(ocbuf<0x10000) obuf[ocbuf++]=symbol;
          else{
            if(fwrite(obuf,1,ocbuf,ofile)<ocbuf||ferror(ofile)) return;
            obuf[0]=symbol;
            ocbuf=1;
          };
          vocbuf[vocroot++]=(uint8_t)symbol;
        };
      };
    };
    *cbuffer<<=1;
    cpos++;
    flags--;
  };
  if(ocbuf) fwrite(obuf,1,ocbuf,ofile);
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
