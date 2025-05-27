#include <stdint.h>
#include <windows.h>

/***********************************************************************************************************/
//Базовая реализация упаковки/распаковки файла по упрощённому алгоритму LZSS+RLE
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
uint8_t vocbuf[0x10000];
uint16_t vocarea[0x10000];
uint16_t hashes[0x10000];
vocpntr vocindx[0x10000];
uint16_t buf_size;
uint16_t voclast;
uint16_t vocroot;
uint16_t offset;
uint16_t length;
DWORD wout,*pwout;

void pack_initialize(){
  flags=buf_size=vocroot=icbuf=wpos=rpos=0;
  pwout=&wout;
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

void wbuf(uint8_t c,HANDLE ofile){
  if(wpos==0x10000&&(wpos=0,!WriteFile(ofile,obuf,0x10000,pwout,NULL))) return;
  obuf[wpos++]=c;
}

void rbuf(uint8_t *c,HANDLE ifile){
  if(rpos==icbuf&&!(rpos=0,ReadFile(ifile,ibuf,0x10000,&icbuf,NULL),icbuf)) return;
  *c=ibuf[rpos++];
}

void pack_file(HANDLE ifile,HANDLE ofile){
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
  WriteFile(ofile,obuf,wpos,pwout,NULL);
}

void unpack_file(HANDLE ifile,HANDLE ofile){
  uint8_t c,rle_flag=0,cflags=0;
  length=0;
  for(;;){
    if(length){
      if(rle_flag) vocbuf[vocroot++]=offset;
      else vocbuf[vocroot++]=vocbuf[offset++];
      length--;
      if(!vocroot&&(!WriteFile(ofile,vocbuf,0x10000,pwout,NULL))) break;
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
  if(length) WriteFile(ofile,vocbuf,vocroot?vocroot:0x10000,pwout,NULL);
}

/***********************************************************************************************************/

void __stdcall start(){
  const LPCSTR msg_a="lz16 file compressor\n\nto   compress use: lz16 c input output\nto decompress use: lz16 d input output\n";
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
