/***********************************************************************************************************/
//Базовая реализация упаковки/распаковки файла по упрощённому алгоритму LZSS+RLE
/***********************************************************************************************************/

#include <cstdint>
#include <algorithm>
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
    uint8_t *ibuf,*obuf,*wpntr,*cbuffer,*vocbuf,flags;
    uint16_t *vocarea,*hashes,buf_size,voclast,vocroot,offset,length,symbol;
    uint32_t icbuf,wpos,rpos;
    vocpntr *vocindx;
    template <class T,class V> void del(T& p,uint32_t s,V v);
    inline void wbuf(uint8_t c);
    inline bool rbuf(uint8_t *c);
    inline void hash(uint16_t s);
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
  if(p){
    fill(p,p+s,v);
    delete[] p;
  };
  p=NULL;
}

packer::packer(){
  ibuf=new uint8_t[0x10000];
  obuf=new uint8_t[0x10000];
  vocbuf=new uint8_t[0x10000];
  cbuffer=new uint8_t[LZ_CAPACITY+1];
  vocarea=new uint16_t[0x10000];
  hashes=new uint16_t[0x10000];
  vocindx=new vocpntr[0x10000];
  wpntr=obuf;
}

packer::~packer(){
  del(ibuf,0x10000,(uint8_t)0);
  del(obuf,0x10000,(uint8_t)0);
  del(vocbuf,0x10000,(uint8_t)0);
  del(cbuffer,LZ_CAPACITY+1,(uint8_t)0);
  del(vocarea,0x10000,(uint16_t)0);
  del(hashes,0x10000,(uint16_t)0);
  del(vocindx,0x10000,(vocpntr){0,0});
  wpntr=NULL;
  buf_size=flags=vocroot=voclast=icbuf=wpos=rpos=0;
}

void packer::init(){
  buf_size=flags=vocroot=*cbuffer=icbuf=wpos=rpos=0;
  voclast=0xfffd;
  for(uint32_t i=0;i<0x10000;i++){
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

inline void packer::wbuf(uint8_t c){
  if(wpos<0x10000){
    *wpntr++=c;
    wpos++;
  }
  else{
    if(ofile.write((char *)obuf,wpos).good()){
      *obuf=c;
      wpos=1;
      wpntr=obuf+1;
      return;
    };
    wpos=0;
  };
}

inline bool packer::rbuf(uint8_t *c){
  if(rpos<icbuf) *c=ibuf[rpos++];
  else{
    rpos=0;
    if(ifile.read((char *)ibuf,0x10000).bad()) return true;
    if((icbuf=ifile.gcount())) *c=ibuf[rpos++];
  };
  return false;
}

inline void packer::hash(uint16_t s){
  uint16_t h=0;
  for(uint8_t i=0;i<4;i++){
    h^=vocbuf[s++];
    h=(h<<4)^(h>>12);
  };
  hashes[voclast]=h;
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
    if(flags==0||eofs){
      *cbuffer<<=flags;
      w=cbuffer;
      for(i=cpos-cbuffer;i;i--){
        wbuf(*w++);
        if(wpos==0) return;
      };
      flags=8;
      cpos=&cbuffer[1];
      if(eofs) break;
    };
  };
  ofile.write((char*)obuf,wpos);
  return;
}

void packer::unpack(){
  uint8_t *cpos=NULL,c,rle_flag=0,bytes=0;
  length=0;
  for(;;){
    if(length){
      if(rle_flag==0) c=vocbuf[offset++];
      vocbuf[vocroot++]=c;
      length--;
      bytes=1;
      if(vocroot==0){
        bytes=0;
        if(ofile.write((char *)vocbuf,0x10000).bad()) break;
      };
    }
    else{
      if(flags==0){
        cpos=cbuffer;
        if(rbuf(cpos++)) break;
        for(c=~*cbuffer;c;flags++) c&=c-1;
        for(c=8+(flags<<1);c;c--)
          if(rbuf(cpos++)) break;
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
    if(vocroot) ofile.write((char *)vocbuf,vocroot);
    else ofile.write((char *)vocbuf,0x10000);
  };
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
