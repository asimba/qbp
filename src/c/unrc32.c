#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#ifndef _WIN32
  #include <unistd.h>
#endif

/***********************************************************************************************************/
//Базовая реализация распаковки файла по упрощённому алгоритму RC32
/***********************************************************************************************************/

uint8_t ibuf[0x10000];
uint8_t obuf[0x10000];
uint8_t cntxs[0x10000];
uint8_t cntx;
uint16_t cmask;
uint32_t icbuf;
uint32_t wpos;
uint32_t rpos;
uint16_t frequency[256][256];
uint16_t fcs[256][17];
uint32_t low;
uint32_t hlp;
uint32_t range;
uint8_t *lowp;
uint8_t *hlpp;

void wbuf(const uint8_t c,FILE *ofile){
  if(wpos==0x10000&&(wpos=0,fwrite(obuf,1,0x10000,ofile)!=0x10000)) return;
  obuf[wpos++]=c;
}

void rbuf(uint8_t *c,FILE *ifile){
  if(rpos==icbuf&&!(rpos=0,icbuf=fread(ibuf,1,0x10000,ifile))) return;
  *c=ibuf[rpos++];
}

uint32_t rc32_getc(uint8_t *c,FILE *ifile){
  cntx=cntxs[cmask];
  uint16_t *_fcs=fcs[cntx],*f=frequency[cntx];
  register uint32_t s=_fcs[16],i,l=0,j=16;
  while(hlp<low||(low^(low+range))<0x1000000||range<s){
    hlp<<=8;
    if(!(rbuf(hlpp,ifile),rpos)) return 0;
    low<<=8;
    range<<=8;
    if(range>~low) range=~low;
  };
  if((i=(hlp-low)/(range/=s))>=s) return 1;
  while(l<j){
    if(_fcs[s=(l+j+1)>>1]<=i) l=s;
    else j=s-1;
  };
  j=l<<4;
  s=_fcs[l++];
  for(;l<17;l++) _fcs[l]++;
  for(;;){
    if((s+=f[j])>i){
      *c=j;
      low+=(s-f[j])*range;
      range*=f[j]++;
      if(!_fcs[16]){
        i=0;
        for(s=0;s<256;s++){
          i+=(f[s]=(f[s]>>1)|(f[s]&1));
          if(!((l=s+1)<<28)) _fcs[l>>4]=i;
        };
      };
      break;
    };
    j++;
  };
  cntxs[cmask]=*c;
  cmask=(cmask<<8)|*c;
  return 0;
}

void unpack_file(FILE *ifile, FILE *ofile){
  uint8_t symbol;
  cntx=low=hlp=icbuf=wpos=rpos=0;
  range=0xffffffff;
  cmask=0xffff;
  lowp=&((uint8_t *)&low)[3];
  hlpp=&((uint8_t *)&hlp)[0];
  for(int i=0;i<0x10000;i++) cntxs[i]=0xff;
  for(int i=0;i<256;i++){
    for(int j=0;j<256;j++) frequency[i][j]=1;
    for(int j=0;j<17;j++) fcs[i][j]=j<<4;
  };
  for(int i=0;i<4;i++){
    hlp<<=8;
    if(!(rbuf(hlpp,ifile),rpos)) return;
  }
  for(;;){
    if(rc32_getc(&symbol,ifile)){
      if(wpos) fwrite(obuf,1,wpos,ofile);
      return;
    }
    else{
      if(!(wbuf(symbol,ofile),wpos)) return;
    };
  };
}

/***********************************************************************************************************/

int main(int argc, char *argv[]){
  if(argc<3){
    printf("rc32 file decompressor\n\nto decompress use: unrc32 input output\n");
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
