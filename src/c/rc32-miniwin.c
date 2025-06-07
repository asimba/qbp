#include <stdint.h>
#include <windows.h>

/***********************************************************************************************************/
//Базовая реализация упаковки/распаковки файла по упрощённому алгоритму RC32
/***********************************************************************************************************/

HANDLE winout;
void msg(LPCSTR text){ WriteConsoleA(winout,text,lstrlenA(text),NULL,NULL); }

uint8_t ibuf[0x10000];
uint8_t obuf[0x10000];
uint8_t cntxs[0x10000];
uint8_t cntx;
uint16_t cmask;
long unsigned int icbuf;
uint32_t wpos;
uint32_t rpos;
uint16_t _frequency[256][260];
uint16_t* frequency[256];
uint16_t fcs[256];
uint32_t low;
uint32_t hlp;
uint32_t range;
uint8_t *lowp;
uint8_t *hlpp;
DWORD wout,*pwout;
HANDLE ifile,ofile;

void pack_initialize(){
  cntx=low=hlp=icbuf=wpos=rpos=0;
  range=0xffffffff;
  cmask=0xffff;
  lowp=&((uint8_t *)&low)[3];
  hlpp=&((uint8_t *)&hlp)[0];
  for(int i=0;i<0x10000;i++) cntxs[i]=0xff;
  for(int i=0;i<256;i++){
    uint64_t *f=(uint64_t *)_frequency[i];
    frequency[i]=_frequency[i]+4;
    f[0]=0;
    for(int j=1;j<65;j++) f[j]=0x0001000100010001ULL;
    fcs[i]=256;
  };
}

void wbuf(const uint8_t c){
  if(wpos==0x10000&&(wpos=0,!WriteFile(ofile,obuf,0x10000,pwout,NULL))) return;
  obuf[wpos++]=c;
}

void rbuf(uint8_t *c){
  if(rpos==icbuf&&!(rpos=0,ReadFile(ifile,ibuf,0x10000,&icbuf,NULL),icbuf)) return;
  *c=ibuf[rpos++];
}

#define rc32_rescale()\
    range*=(*f)++;\
    if(!++fc){\
      f=frequency[cntx];\
      for(s=0;s<256;s++) fc+=(*f=((*f)>>1)|(*f&1)),f++;\
    };\
    fcs[cntx]=fc;

#define rc32_shift() low<<=8; range<<=8; if(range>~low) range=~low;

uint32_t rc32_getc(uint8_t *c){
  cntx=cntxs[cmask];
  uint16_t fc=fcs[cntx];
  while(hlp<low||(low^(low+range))<0x1000000||range<fc){
    hlp<<=8;
    if(!(rbuf(hlpp),rpos)) return 0;
    rc32_shift();
  };
  uint32_t i;
  if((i=(hlp-low)/(range/=fc))>=fc) return 1;
  uint16_t *f=frequency[cntx];
  register uint64_t s=0;
  while((s+=*f)<=i) f++;
  low+=(s-*f)*range;
  *c=f-frequency[cntx];
  rc32_rescale();
  cntxs[cmask]=*c;
  cmask=(cmask<<8)|*c;
  return 0;
}

uint32_t rc32_putc(uint32_t c){
  cntx=cntxs[cmask];
  uint16_t fc=fcs[cntx];
  uint8_t t=c;
  while((low^(low+range))<0x1000000||range<fc){
    if(!(wbuf(*lowp),wpos)) return 1;
    rc32_shift();
  };
  if(c==256){
    low+=(range/fc)*fc;
    return 0;
  };
  uint16_t *f=_frequency[cntx]+(c&3);
  register uint64_t s=0;
  c=(c>>2)+1;
  while(c--) s+=*(uint64_t *)f,f+=4;
  s+=s>>32;
  low+=((uint16_t)(s+(s>>16)))*(range/=fc);
  rc32_rescale();
  cntxs[cmask]=t;
  cmask=(cmask<<8)|t;
  return 0;
}

void pack_file(){
  uint8_t symbol=0;
  for(;;){
    if((rbuf(&symbol),rpos)){
      if(rc32_putc(symbol)) return;
    }
    else{
      if(rc32_putc(256)) return;
      for(int i=4;i;i--){
        if(!(wbuf(*lowp),wpos)) return;
        low<<=8;
      };
      WriteFile(ofile,obuf,wpos,pwout,NULL);
      break;
    };
  };
}

void unpack_file(){
  uint8_t symbol;
  for(int i=0;i<4;i++){
    hlp<<=8;
    if(!(rbuf(hlpp),rpos)) return;
  }
  for(;;){
    if(rc32_getc(&symbol)){
      if(wpos) WriteFile(ofile,obuf,wpos,pwout,NULL);
      return;
    }
    else{
      if(!(wbuf(symbol),wpos)) return;
    };
  };
}

/***********************************************************************************************************/

void __stdcall start(){
  const LPCSTR msg_a="rc32 file compressor\n\nto   compress use: rc32 c input output\nto decompress use: rc32 d input output\n";
  const LPCSTR msg_b="Error: unable to open input file!\n";
  const LPCSTR msg_c="Error: unable to open output file or output file already exists!\n";
  winout=GetStdHandle(STD_OUTPUT_HANDLE);
  int argc=0;
  LPWSTR *argv=CommandLineToArgvW(GetCommandLineW(),&argc);
  if(argv){
    if(argc>3&&((argv[1][0]==L'c')||(argv[1][0]==L'd'))){
      if((ifile=CreateFileW(argv[2],GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL))!=INVALID_HANDLE_VALUE){
        if((ofile=CreateFileW(argv[3],GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL))!=INVALID_HANDLE_VALUE){
          pack_initialize();
          SetProcessWorkingSetSize(GetCurrentProcess(),(SIZE_T)-1,(SIZE_T)-1);
          if(argv[1][0]==L'c') pack_file();
          else unpack_file();
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
