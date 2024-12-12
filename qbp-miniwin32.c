#include <stdint.h>
#include <windows.h>

/***********************************************************************************************************/
//Базовая реализация упаковки/распаковки файла по упрощённому алгоритму LZSS+RLE+RC32
/***********************************************************************************************************/

#define LZ_BUF_SIZE 259
#define LZ_CAPACITY 24
#define LZ_MIN_MATCH 3

HANDLE winout;
void msg(LPCSTR text){ WriteConsoleA(winout,text,lstrlenA(text),NULL,NULL); }

typedef union{
  struct{
    uint16_t in;
    uint16_t out;
  };
  uint32_t val;
} vocpntr;

uint8_t ibuf[0x10000];
uint8_t obuf[0x10000];
long unsigned int icbuf;
uint32_t wpos;
uint32_t rpos;
uint8_t flags;
uint8_t cbuffer[LZ_CAPACITY+1];
uint8_t cntxs[LZ_CAPACITY+2];
uint8_t vocbuf[0x10000];
uint16_t vocarea[0x10000];
uint16_t hashes[0x10000];
vocpntr vocindx[0x10000];
uint16_t _frequency[256][260];
uint16_t* frequency[256];
uint16_t fcs[256];
uint16_t buf_size;
uint16_t voclast;
uint16_t vocroot;
uint16_t length;
uint16_t offset;
uint32_t low;
uint32_t hlp;
uint32_t range;
uint8_t *lowp;
uint8_t *hlpp;
uint8_t scntx;
DWORD wout,*pwout;

void pack_initialize(){
  cntxs[0]=flags=buf_size=vocroot=low=hlp=icbuf=wpos=rpos=0;
  pwout=&wout;
  voclast=0xfffc;
  range=0xffffffff;
  lowp=&((uint8_t *)&low)[3];
  hlpp=&((uint8_t *)&hlp)[0];
  for(int i=0;i<256;i++){
    uint64_t *f=(uint64_t *)_frequency[i];
    frequency[i]=_frequency[i]+4;
    f[0]=0;
    for(int j=1;j<65;j++) f[j]=0x0001000100010001ULL;
    fcs[i]=256;
  };
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
  scntx=0xff;
}

void wbuf(uint8_t c,HANDLE ofile){
  if(wpos==0x10000&&(wpos=0,!WriteFile(ofile,obuf,0x10000,pwout,NULL))) return;
  obuf[wpos++]=c;
}

void rbuf(uint8_t *c,HANDLE ifile){
  if(rpos==icbuf&&!(rpos=0,ReadFile(ifile,ibuf,0x10000,&icbuf,NULL),icbuf)) return;
  *c=ibuf[rpos++];
}

#define rc32_rescale()\
    range*=(*f)++;\
    if(!++fc){\
      f=frequency[cntx];\
      for(s=0;s<256;s++) fc+=(*f=((*f)>>1)|(*f&1)),f++;\
    };\
    fcs[cntx]=fc;\
    return 0;

#define rc32_shift() low<<=8;if((uint32_t)((range<<=8)+low)<low) range=~low;

uint32_t rc32_getc(uint8_t *c,HANDLE ifile,uint8_t cntx){
  while(hlp<low||(low^(low+range))<0x1000000||range<0x10000){
    hlp<<=8;
    if(!(rbuf(hlpp,ifile),rpos)) return 0;
    rc32_shift();
  };
  uint16_t *f=frequency[cntx],fc=fcs[cntx];
  uint32_t i;
  if((i=(hlp-low)/(range/=fc))>=fc) return 1;
  register uint32_t s=0;
  while((s+=*f)<=i) f++;
  low+=(s-*f)*range;
  *c=f-frequency[cntx];
  rc32_rescale();
}

uint32_t rc32_putc(uint32_t c,HANDLE ofile,uint8_t cntx){
  while((low^(low+range))<0x1000000||range<0x10000){
    if(!(wbuf(*lowp,ofile),wpos)) return 1;
    rc32_shift();
  };
  uint16_t *f=_frequency[cntx]+(c&3),fc=fcs[cntx];
  register uint64_t s=0;
  c=(c>>2)+1;
  while(c--) s+=*(uint64_t *)f,f+=4;
  s+=s>>32;
  low+=((uint16_t)(s+(s>>16)))*(range/=fc);
  rc32_rescale();
}

void pack_file(HANDLE ifile,HANDLE ofile){
  uint8_t *cpos=&cbuffer[1],*npos=&cntxs[1],eoff=0;
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
        *(uint32_t*)npos=0x00030201;
        npos+=3;
        *cpos++=--i;
        *(uint16_t*)cpos++=offset;
        buf_size-=length;
      }
      else{
        *npos++=scntx;
        scntx=vocbuf[symbol];
        *cbuffer|=1;
        *cpos=vocbuf[symbol];
        buf_size--;
      };
    }
    else{
      *(uint32_t*)npos=0x00030201;
      length=0;
      cpos++;
      *(uint16_t*)cpos++=0x0100;
    };
    cpos++;
    if(--flags&&length) continue;
    *cbuffer<<=flags;
    for(int i=0;i<cpos-cbuffer;i++)
      if(rc32_putc(cbuffer[i],ofile,cntxs[i])) return;
    if(!length){
      for(int i=4;i;i--){
        if(!(wbuf(*lowp,ofile),wpos)) return;
        low<<=8;
      };
      WriteFile(ofile,obuf,wpos,pwout,NULL);
      break;
    };
    cpos=&cbuffer[1];
    npos=&cntxs[1];
    flags=8;
  };
}

void unpack_file(HANDLE ifile,HANDLE ofile){
  uint8_t *cpos=NULL,c,rle_flag=0,bytes=0,cntx,cflags;
  length=0;
  for(c=0;c<4;c++){
    hlp<<=8;
    if(!(rbuf(hlpp,ifile),rpos)) return;
  }
  for(;;){
    if(length){
      if(!rle_flag) c=vocbuf[offset++];
      vocbuf[vocroot++]=c;
      length--;
      bytes=1;
      if(!vocroot&&(bytes=0,!WriteFile(ofile,vocbuf,0x10000,pwout,NULL))) break;
      continue;
    };
    if(flags){
      length=rle_flag=1;
      if(*cbuffer&0x80) c=*cpos;
      else{
        length=LZ_MIN_MATCH+1+*cpos++;
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
    if(rc32_getc(cpos++,ifile,0)) break;
    for(c=~*cbuffer;c;flags++) c&=c-1;
    cflags=*cbuffer;
    for(c=0;c<(cntx=8+(flags<<1));){
      if(cflags&0x80) cntxs[c++]=4;
      else{
        *(uint32_t*)(cntxs+c)=0x00030201;
        c+=3;
      };
      cflags<<=1;
    };
    for(c=0;c<cntx;c++)
      if(cntxs[c]==4){
        if(rc32_getc(cpos,ifile,scntx)) return;
        scntx=*cpos++;
      }
      else{
        if(rc32_getc(cpos++,ifile,cntxs[c])) return;
      };
    cpos=&cbuffer[1];
    flags=8;
  };
  if(bytes) WriteFile(ofile,vocbuf,vocroot?vocroot:0x10000,pwout,NULL);
}

/***********************************************************************************************************/


void __stdcall start(){
  const LPCSTR msg_a="qbp file compressor\n\nto   compress use: qbp c input output\nto decompress use: qbp d input output\n";
  const LPCSTR msg_b="Error: unable to open input file!\n";
  const LPCSTR msg_c="Error: unable to open output file or output file already exists!\n";
  winout=GetStdHandle(STD_OUTPUT_HANDLE);
  int argc=0;
  LPWSTR *argv=CommandLineToArgvW(GetCommandLineW(),&argc);
  if(argv){
    if(argc>3&&((argv[1][0]==L'c')||(argv[1][0]==L'd'))){
      HANDLE ifile;
      if((ifile=CreateFileW(argv[2],GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL))!=INVALID_HANDLE_VALUE){
        HANDLE ofile;
        if((ofile=CreateFileW(argv[3],GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL))!=INVALID_HANDLE_VALUE){
          pack_initialize();
          SetProcessWorkingSetSize(GetCurrentProcess(),(SIZE_T)-1,(SIZE_T)-1);
          if(argv[1][0]==L'c') pack_file(ifile,ofile);
          else unpack_file(ifile,ofile);
          CloseHandle(ofile);
        }
        else msg(msg_c);
        CloseHandle(ifile);
      }
      else msg(msg_b);
    }
    else msg(msg_a);
    LocalFree(argv);
  };
}
