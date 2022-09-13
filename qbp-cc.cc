/***********************************************************************************************************/
//Базовая реализация упаковки/распаковки файла по упрощённому алгоритму LZSS+RLE+RC32
/***********************************************************************************************************/

#include <cstdint>
#include <algorithm>
#include <filesystem>
#include <fstream>

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
    uint8_t *ibuf,*obuf,*cbuffer,*vocbuf,*lowp,*hlpp,flags;
    uint16_t *vocarea,*hashes,buf_size,voclast,vocroot,offset,lenght,symbol,fc,*frequency;
    uint32_t icbuf,wpos,rpos,low,hlp,range;
    vocpntr *vocindx;
    template <class T,class V> void del(T& p,uint32_t s,V v);
    inline uint8_t wbuf(uint8_t c);
    inline uint8_t rbuf(uint8_t *c);
    inline uint16_t hash(uint16_t s);
    inline void rc32_rescale(uint32_t s);
    inline uint8_t rc32_getc(uint8_t *c);
    inline uint8_t rc32_putc(uint8_t c);
  public:
    ifstream ifile;
    ofstream ofile;
    void init();
    void pack();
    void unpack();
    packer();
    ~packer();
};

template <class T,class V> void packer::del(T& p,uint32_t s,V v){
  try{
    if(p){
      fill(p,p+s,v);
      delete[] p;
    };
    p=NULL;
  }
  catch(...){};
}

packer::packer(){
    ibuf=new uint8_t[0x10000];
    obuf=new uint8_t[0x10000];
    vocbuf=new uint8_t[0x10000];
    cbuffer=new uint8_t[LZ_CAPACITY+1];
    vocarea=new uint16_t[0x10000];
    hashes=new uint16_t[0x10000];
    vocindx=new vocpntr[0x10000];
    frequency=new uint16_t[256];
    lowp=&((uint8_t *)&low)[3];
    hlpp=&((uint8_t *)&hlp)[0];
}

packer::~packer(){
  del(ibuf,0x10000,(uint8_t)0);
  del(obuf,0x10000,(uint8_t)0);
  del(vocbuf,0x10000,(uint8_t)0);
  del(cbuffer,LZ_CAPACITY+1,(uint8_t)0);
  del(vocarea,0x10000,(uint16_t)0);
  del(hashes,0x10000,(uint16_t)0);
  del(frequency,256,(uint16_t)0);
  del(vocindx,0x10000,(vocpntr){0,0});
  lowp=NULL;
  hlpp=NULL;
  fc=0;
  buf_size=flags=vocroot=voclast=range=low=hlp=icbuf=wpos=rpos=0;
}

void packer::init(){
  buf_size=flags=vocroot=*cbuffer=low=hlp=icbuf=wpos=rpos=0;
  voclast=0xfffd;
  range=0xffffffff;
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
}

inline uint8_t packer::wbuf(uint8_t c){
  if(wpos<0x10000) obuf[wpos++]=c;
  else{
    ofile.write((char *)obuf,wpos);
    if(!ofile.good()) return 1;
    *obuf=c;
    wpos=1;
  };
  return 0;
}

inline uint8_t packer::rbuf(uint8_t *c){
  if(icbuf--) *c=ibuf[rpos++];
  else{
    ifile.read((char *)ibuf,0x10000);
    icbuf=ifile.gcount();
    if(!ifile.good()&&!ifile.eof()) return 1;
    if(icbuf){
      *c=*ibuf;
      rpos=1;
      icbuf--;
    }
    else rpos=0;
  };
  return 0;
}

inline uint16_t packer::hash(uint16_t s){
  uint16_t h=0;
  for(uint8_t i=0;i<sizeof(uint32_t);i++){
    h^=vocbuf[s++];
    h=(h<<4)^(h>>12);
  };
  return h;
}

inline void packer::rc32_rescale(uint32_t s){
  low+=s*range;
  range*=frequency[symbol]++;
  if(++fc==0){
    for(uint16_t i=0;i<256;i++){
      if((frequency[i]>>=1)==0) frequency[i]=1;
      fc+=frequency[i];
    };
  };
}

inline uint8_t packer::rc32_getc(uint8_t *c){
  while((range<0x10000)||(hlp<low)){
    hlp<<=8;
    if(rbuf(hlpp)) return 1;
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
  s-=frequency[symbol];
  *c=(uint8_t)symbol;
  rc32_rescale(s);
  return 0;
}

inline uint8_t packer::rc32_putc(uint8_t c){
  while(range<0x10000){
    if(wbuf(*lowp)) return 1;
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

void packer::pack(){
  uint16_t i,rle,rle_shift,cnode;
  uint8_t *cpos=&cbuffer[1],*w;
  char eoff=0,eofs=0;
  vocpntr *indx;
  flags=8;
  for(;;){
    if(!eoff){
      if(LZ_BUF_SIZE-buf_size){
        if(rbuf(&vocbuf[vocroot])) break;
        if(rpos==0){
          eoff=1;
          continue;
        }
        else{
          if(vocarea[vocroot]==vocroot) vocindx[hashes[vocroot]].val=1;
          else vocindx[hashes[vocroot]].in=vocarea[vocroot];
          vocarea[vocroot]=vocroot;
          hashes[voclast]=hash(voclast);
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
      lenght=LZ_MIN_MATCH;
      if(buf_size>LZ_MIN_MATCH){
        cnode=vocindx[hashes[symbol]].in;
        rle_shift=(uint16_t)(vocroot+LZ_BUF_SIZE-buf_size);
        while(cnode!=symbol){
          if(vocbuf[(uint16_t)(symbol+lenght)]==vocbuf[(uint16_t)(cnode+lenght)]){
            i=0;
            uint16_t j=symbol,k=cnode;
            while(vocbuf[j++]==vocbuf[k]&&k++!=symbol) i++;
            if(i>=lenght){
              //while buf_size==LZ_BUF_SIZE: minimal offset > 0x0104;
              if(buf_size<LZ_BUF_SIZE){
                if((uint16_t)(cnode-rle_shift)>0xfeff){
                  cnode=vocarea[cnode];
                  continue;
                };
              };
              offset=cnode;
              if(i>=buf_size){
                lenght=buf_size;
                break;
              }
              else lenght=i;
            };
          };
          cnode=vocarea[cnode];
        };
      };
      if(rle>lenght){
        *cpos++=rle-LZ_MIN_MATCH-1;
        *(uint16_t*)cpos++=((uint16_t)(vocbuf[symbol]));
        buf_size-=rle;
      }
      else{
        if(lenght>LZ_MIN_MATCH){
          *cpos++=lenght-LZ_MIN_MATCH-1;
          *(uint16_t*)cpos++=0xffff-(uint16_t)(offset-rle_shift);
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
        if(rc32_putc(*w++)) return;
      flags=8;
      cpos=&cbuffer[1];
      if(eofs){
        for(i=sizeof(uint32_t);i;i--){
          if(wbuf(*lowp)) return;
          low<<=8;
        }
        ofile.write((char*)obuf,wpos);
        break;
      };
    };
  };
  return;
}

void packer::unpack(){
  uint16_t i;
  uint8_t *cpos=cbuffer,c;
  for(i=0;i<sizeof(uint32_t);i++){
    hlp<<=8;
    if(rbuf(hlpp)||rpos==0) return;
  }
  for(;;){
    if(!flags){
      cpos=cbuffer;
      if(rc32_getc(cpos++)) return;
      flags=8;
      lenght=0;
      c=*cbuffer;
      for(i=0;i<flags;i++){
        if(c&0x80) lenght++;
        else lenght+=3;
        c<<=1;
      };
      for(i=lenght;i;i--)
        if(rc32_getc(cpos++)) return;
      cpos=cbuffer+1;
    };
    if(*cbuffer&0x80){
      if(wbuf(*cpos)) break;
      vocbuf[vocroot++]=*cpos;
    }
    else{
      lenght=*cpos++;
      lenght+=LZ_MIN_MATCH+1;
      offset=*(uint16_t*)cpos++;
      if(offset==0x0100) break;
      if(offset<0x0100){
        c=(uint8_t)(offset);
        for(i=0;i<lenght;i++){
          if(wbuf(c)) return;
          vocbuf[vocroot++]=c;
        };
      }
      else{
        offset=0xffff-offset+(uint16_t)(vocroot+LZ_BUF_SIZE);
        for(i=0;i<lenght;i++){
          c=vocbuf[offset++];
          if(wbuf(c)) return;
          vocbuf[vocroot++]=c;
        };
      };
    };
    *cbuffer<<=1;
    cpos++;
    flags--;
  };
  ofile.write((char*)obuf,wpos);
  return;
}

int main(int argc, char *argv[]){
  packer *p;
  if((argc<4)||(ifstream(argv[3]).good())||\
     ((argv[1][0]!='c')&&(argv[1][0]!='d')))
    goto rpoint00;
  p=new packer();
  p->ifile.open(argv[2],fstream::in|fstream::binary);
  if(!p->ifile.is_open()) goto rpoint01;
  p->ofile.open(argv[3],fstream::out|fstream::binary);
  if(!p->ofile.is_open()) goto rpoint02;
  p->init();
  if(argv[1][0]=='c') p->pack();
  else p->unpack();
  p->ofile.close();
rpoint02:
  p->ifile.close();
rpoint01:
  delete p;
rpoint00:
  return 0;
}
