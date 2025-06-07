/***********************************************************************************************************/
//Базовая реализация упаковки/распаковки файла по упрощённому алгоритму LZSS+RLE+RC32
/***********************************************************************************************************/

#include <cstdint>
#include <algorithm>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

typedef int32_t (*io_operator)(void *optional, char *buffer, int32_t length);

int32_t read_operator(void *file, char *buffer, int32_t length){
    int32_t ret=0;
    if((ret=fread(buffer,sizeof(uint8_t),length,(FILE*)file))==0){
      if(ferror((FILE*)file)) return -1;
    };
    return ret;
}

int32_t write_operator(void *file, char *buffer, int32_t length){
    int32_t ret=0;
    if(length&&((ret=fwrite(buffer,sizeof(uint8_t),length,(FILE*)file))==0)) return -1;
    return ret;
}

using namespace std;

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

class packer{
  private:
    uint8_t *iobuf;
    uint8_t *cbuffer;
    uint8_t *cntxs;
    uint8_t *vocbuf;
    uint8_t *lowp;
    uint8_t *hlpp;
    uint8_t *cpos;
    uint8_t rle_flag;
    uint16_t *vocarea;
    uint16_t *hashes;
    uint16_t buf_size;
    uint16_t voclast;
    uint16_t vocroot;
    uint16_t offset;
    uint16_t length;
    uint16_t symbol;
    uint16_t *fcs;
    uint16_t **_frequency;
    uint16_t **frequency;
    int32_t icbuf;
    uint32_t wpos;
    uint32_t rpos;
    uint32_t low;
    uint32_t hlp;
    uint32_t range;
    vocpntr *vocindx;
    template <class T,class V> void del(T& p,uint32_t s,V v);
    io_operator read;
    io_operator write;
    inline void wbuf(void* file, const uint8_t c);
    inline bool rbuf(void* file, uint8_t *c);
    inline bool rc32_getc(void* file, uint8_t *c, const uint8_t cntx);
    inline bool rc32_putc(void* file, uint32_t c, const uint8_t cntx);
  public:
    uint8_t flags;
    uint8_t cnt;
    bool finalize;
    void set_operators(io_operator r, io_operator w);
    void init();
    bool load_header(void* file);
    bool packer_putc(void* file, uint8_t c);
    bool packer_getc(void* file, uint8_t *c);
    int32_t packer_read(void* file, char *buf, int32_t l);
    int32_t packer_write(void* file, char *buf, int32_t l);
    packer();
    ~packer();
};

template <class T,class V> void packer::del(T& p,uint32_t s,V v){
  if(p){
    fill(p,p+s,v);
    delete[] p;
  };
  p=NULL;
}

packer::packer(){
  iobuf=new uint8_t[0x10000];
  vocbuf=new uint8_t[0x10000];
  cbuffer=new uint8_t[LZ_CAPACITY+1];
  cntxs=new uint8_t[LZ_CAPACITY+1];
  vocarea=new uint16_t[0x10000];
  hashes=new uint16_t[0x10000];
  vocindx=new vocpntr[0x10000];
  _frequency=new uint16_t*[256];
  frequency=new uint16_t*[256];
  for(int i=0;i<256;i++) _frequency[i]=new uint16_t[260];
  fcs=new uint16_t[256];
  lowp=&((uint8_t *)&low)[3];
  hlpp=&((uint8_t *)&hlp)[0];
  read=NULL;
  write=NULL;
}

packer::~packer(){
  del(iobuf,0x10000,(uint8_t)0);
  del(vocbuf,0x10000,(uint8_t)0);
  del(cbuffer,LZ_CAPACITY+1,(uint8_t)0);
  del(cntxs,LZ_CAPACITY+1,(uint8_t)0);
  del(vocarea,0x10000,(uint16_t)0);
  del(hashes,0x10000,(uint16_t)0);
  for(int i=0;i<256;i++) del(_frequency[i],260,(uint16_t)0);
  delete[] _frequency;
  del(frequency,256,(uint16_t*)NULL);
  del(fcs,256,(uint16_t)0);
  del(vocindx,0x10000,(vocpntr){0,0});
  lowp=NULL;
  hlpp=NULL;
  read=NULL;
  write=NULL;
  cnt=flags=buf_size=vocroot=voclast=range=low=hlp=icbuf=wpos=rpos=0;
}

void packer::set_operators(io_operator r, io_operator w){
  if(r) read=r;
  if(w) write=w;
}

void packer::init(){
  cntxs[0]=flags=rle_flag=length=buf_size=vocroot=*cbuffer=low=hlp=icbuf=wpos=rpos=0;
  cnt=1;
  voclast=0xfffc;
  range=0xffffffff;
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
  cpos=&cbuffer[1];
  finalize=false;
}

inline void packer::wbuf(void *file, const uint8_t c){
  if(wpos==0x10000&&(wpos=0,(*write)(file,(char*)iobuf,0x10000)<0)) return;
  iobuf[wpos++]=c;
}

inline bool packer::rbuf(void *file, uint8_t *c){
  if((int32_t)rpos==icbuf&&(rpos=0,(icbuf=(*read)(file,(char*)iobuf,0x10000))<0)) return true;
  if(icbuf) *c=iobuf[rpos++];
  return false;
}

#define rc32_rescale()\
    range*=(*f)++;\
    if(!++fc){\
      f=frequency[cntx];\
      for(s=0;s<256;s++) fc+=(*f=((*f)>>1)|(*f&1)),f++;\
    };\
    fcs[cntx]=fc;\
    return false;

#define rc32_shift() low<<=8; range<<=8; if(range>~low) range=~low;

inline bool packer::rc32_getc(void *file, uint8_t *c, const uint8_t cntx){
  uint16_t fc=fcs[cntx];
  while(hlp<low||(low^(low+range))<0x1000000||range<fc){
    hlp<<=8;
    if(rbuf(file,hlpp)) return true;
    if(!rpos) return false;
    rc32_shift();
  };
  uint32_t i;
  if((i=(hlp-low)/(range/=fc))>=fc) return true;
  uint16_t *f=frequency[cntx];
  register uint64_t s=0;
  while((s+=*f)<=i) f++;
  low+=(s-*f)*range;
  *c=f-frequency[cntx];
  rc32_rescale();
}

inline bool packer::rc32_putc(void *file, uint32_t c, const uint8_t cntx){
  uint16_t fc=fcs[cntx];
  while((low^(low+range))<0x1000000||range<fc){
    if(!(wbuf(file,*lowp),wpos)) return true;
    rc32_shift();
  };
  uint16_t *f=_frequency[cntx]+(c&3);
  register uint64_t s=0;
  c=(c>>2)+1;
  while(c--) s+=*(uint64_t *)f,f+=4;
  s+=s>>32;
  low+=((uint16_t)(s+(s>>16)))*(range/=fc);
  rc32_rescale();
}

bool packer::packer_putc(void *file, uint8_t c){
  for(;;){
    if(!finalize&&buf_size!=LZ_BUF_SIZE){
      vocbuf[vocroot]=c;
      if(vocarea[vocroot]==vocroot) vocindx[hashes[vocroot]].val=1;
      else vocindx[hashes[vocroot]].in=vocarea[vocroot];
      vocarea[vocroot]=vocroot;
      uint16_t hs=hashes[voclast]^vocbuf[voclast]^vocbuf[vocroot++];
      vocpntr *indx=&vocindx[(hashes[++voclast]=(hs<<4)|(hs>>12))];
      if(indx->val==1) indx->in=voclast;
      else vocarea[indx->out]=voclast;
      indx->out=voclast;
      buf_size++;
      return false;
    }
    else{
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
          cntxs[cnt++]=1;
          cntxs[cnt++]=2;
          cntxs[cnt++]=3;
          *cpos++=--i;
          *cpos++=(uint8_t)offset;
          *cpos=(uint8_t)(offset>>8);
          buf_size-=length;
        }
        else{
          cntxs[cnt++]=vocbuf[(uint16_t)(symbol-1)];
          *cpos=vocbuf[symbol];
          *cbuffer|=1;
          buf_size--;
        };
      }
      else{
        cntxs[cnt++]=1;
        cntxs[cnt++]=2;
        cntxs[cnt++]=3;
        length=0;
        cpos++;
        *cpos++=0;
        *cpos=1;
      };
      cpos++;
      if(--flags&&length) continue;
      *cbuffer<<=flags;
      for(int i=0;i<cpos-cbuffer;i++)
        if(rc32_putc(file,cbuffer[i],cntxs[i])) return true;
      if(!length){
        for(int i=4;i;i--){
          if(!(wbuf(file,*lowp),wpos)) return true;
          low<<=8;
        };
        if((*write)(file,(char*)iobuf,wpos)<=0) return true;
        break;
      };
      cpos=&cbuffer[1];
      cnt=1;
      flags=8;
    };
  };
  return false;
}

bool packer::load_header(void *file){
  for(unsigned int i=0;i<sizeof(uint32_t);i++){
    hlp<<=8;
    if(rbuf(file,hlpp)||!rpos) return true;
  };
  return false;
}

bool packer::packer_getc(void *file, uint8_t *c){
  for(;;){
    if(length){
      if(rle_flag) *c=vocbuf[vocroot++]=offset;
      else *c=vocbuf[vocroot++]=vocbuf[offset++];
      length--;
      return false;
    };
    if(flags){
      length=rle_flag=1;
      uint8_t i=0;
      if(*cbuffer&0x80){
        if(rc32_getc(file,&i,vocbuf[(uint16_t)(vocroot-1)])) return true;
        offset=i;
      }
      else{
        for(i=1;i<4;i++)
          if(rc32_getc(file,cbuffer+i,i)) return true;
        length=LZ_MIN_MATCH+1+cbuffer[1];
        offset=cbuffer[2];
        offset|=((uint16_t)cbuffer[3])<<8;
        if(offset>=0x0100){
          if(offset==0x0100){
            finalize=true;
            break;
          };
          offset=~offset+vocroot+LZ_BUF_SIZE;
          rle_flag=0;
        };
      };
      cbuffer[0]<<=1;
      flags<<=1;
      continue;
    };
    if(rc32_getc(file,cbuffer,0)) return true;
    flags=0xff;
  };
  return false;
}

int32_t packer::packer_read(void* file, char *buf, int32_t l){
  if(read==NULL||write==NULL) return -1;
  int32_t ret=0;
  while(l--){
    if(packer_getc(file,(uint8_t *)buf++)) return -1;
    if(finalize) return 0;
    else ret++;
  };
  return ret;
}

int32_t packer::packer_write(void* file, char *buf, int32_t l){
  if(read==NULL||write==NULL) return -1;
  if(buf==NULL||l==0){
    finalize=true;
    packer_putc(file,0);
  }
  else{
    while(l--)
      if(packer_putc(file,*buf++)) return -1;
  };
  return 1;
}

void pack(packer *p, void *ifile, void *ofile){
  char buffer[10000];
  int32_t l=0;
  p->init();
  p->set_operators(read_operator,write_operator);
  p->flags=8;
  while(p->finalize==false){
    if((l=fread(buffer,1,10000,(FILE*)ifile))==0){
      if(ferror((FILE*)ifile)) return;
    };
    if(l==0) p->finalize=true;
    if(p->packer_write(ofile,buffer,l)<0) return;
  };
}

void unpack(packer *p, void *ifile, void *ofile){
  char buffer[10000];
  int32_t l=0;
  p->init();
  p->set_operators(read_operator,write_operator);
  if(p->load_header(ifile)) return;
  for(;;){
    if((l=p->packer_read(ifile,buffer,10000))<0) return;
    if(l&&(fwrite(buffer,1,l,(FILE*)ofile)==0)) return;
    if(p->finalize) break;
  };
}

int main(int argc, char *argv[]){
  if((argc<4)||((argv[1][0]!='c')&&(argv[1][0]!='d'))){
    printf("qbp-cc file compressor\n\nto   compress use: qbp-cc c input output\nto decompress use: qbp-cc d input output\n");
    goto rpoint00;
  };
  if(access(argv[3],0)==0){
    printf("Error: output file already exists!\n");
    goto rpoint00;
  };
  FILE *ifile,*ofile;
  if(!(ifile=fopen(argv[2],"rb"))) goto rpoint00;
  if(!(ofile=fopen(argv[3],"wb"))) goto rpoint01;
  packer *p;
  p=new packer();
  if(argv[1][0]=='c') pack(p,(void *)ifile,(void *)ofile);
  else unpack(p,(void *)ifile,(void *)ofile);
  fclose(ofile);
rpoint01:
  fclose(ifile);
rpoint00:
  return 0;
}
