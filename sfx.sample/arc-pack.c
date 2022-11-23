#include <dirent.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
  #include <unistd.h>
#endif

#define LZ_BUF_SIZE 259
#define LZ_CAPACITY 24
#define LZ_MIN_MATCH 3

typedef struct{
  uint16_t in;
  uint16_t out;
} vocpntr;

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
char *lowp;
char *hlpp;

void pack_initialize(){
  buf_size=flags=vocroot=*cbuffer=low=hlp=0;
  voclast=0xfffd;
  range=0xffffffff;
  lowp=&((char *)&low)[3];
  hlpp=&((char *)&hlp)[0];
  uint32_t i;
  for(i=0;i<256;i++) frequency[i]=1;
  fc=256;
  for(i=0;i<0x10000;i++){
    vocbuf[i]=0xff;
    hashes[i]=0;
    vocindx[i].in=1;
    vocindx[i].out=0;
    vocarea[i]=(uint16_t)(i+1);
  };
  vocindx[0].in=0;
  vocindx[0].out=0xfffc;
  vocarea[0xfffc]=0xfffc;
  vocarea[0xfffd]=0xfffd;
  vocarea[0xfffe]=0xfffe;
  vocarea[0xffff]=0xffff;
}

void rc32_rescale(uint32_t s){
  low+=s*range;
  range*=frequency[symbol]++;
  if(++fc==0){
    for(uint16_t i=0;i<256;i++){
      if((frequency[i]>>=4)==0) frequency[i]=1;
      fc+=frequency[i];
    };
  };
}

int rc32_write(uint8_t *buf,int l,FILE *ofile){
  while(l--){
    while(range<0x10000){
      fputc(*lowp,ofile);
      if(ferror(ofile)) return -1;
      low<<=8;
      range<<=8;
      if((uint32_t)(range+low)<low) range=~low;
    };
    symbol=*buf;
    range/=fc;
    uint32_t s=0;
    for(uint16_t i=0;i<symbol;i++) s+=frequency[i];
    rc32_rescale(s);
    buf++;
  };
  return 1;
}

uint16_t hash(uint16_t s){
  uint16_t h=0;
  for(uint8_t i=0;i<sizeof(uint32_t);i++){
    h^=vocbuf[s++];
    h=(h<<4)^(h>>12);
  };
  return h;
}

void pack_file(FILE *ifile,FILE *ofile){
  uint16_t i,rle,rle_shift,cnode;
  char eoff=0,eofs=0;
  vocpntr *indx;
  uint8_t *cpos=&cbuffer[1];
  flags=8;
  for(;;){
    if(!eoff){
      if(LZ_BUF_SIZE-buf_size){
        vocbuf[vocroot]=fgetc(ifile);
        if(ferror(ifile)) break;
        if(feof(ifile)) eoff=1;
        else{
          if(vocarea[vocroot]==vocroot){
            vocindx[hashes[vocroot]].in=1;
            vocindx[hashes[vocroot]].out=0;
          }
          else vocindx[hashes[vocroot]].in=vocarea[vocroot];
          vocarea[vocroot]=vocroot;
          hashes[voclast]=hash(voclast);
          indx=&vocindx[hashes[voclast]];
          if(indx->in==1&&indx->out==0) indx->in=voclast;
          else vocarea[indx->out]=voclast;
          indx->out=voclast;
          voclast++;
          vocroot++;
          buf_size++;
          continue;
        };
      };
    };
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
      if(rc32_write(cbuffer,cpos-cbuffer,ofile)<0) break;
      flags=8;
      cpos=&cbuffer[1];
      if(eofs){
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

void list(char *path,FILE *of){
  DIR *d=opendir(path);
  if(!d) return;
  struct dirent *dir;
  char fp[257];
  struct stat st;
  while((dir=readdir(d))!=NULL){
      sprintf(fp,"%s/%s",path,dir->d_name);
      stat(fp,&st);
      if(!S_ISDIR(st.st_mode)){
        uint32_t fl=strlen(fp);
        uint32_t fs=st.st_size;
        printf("%u,%s,%u\n",fl,fp,fs);
        char *t;
        t=(char *)&fl;
        for(int i=0;i<sizeof(uint32_t);i++){
          fputc(t[i],of);
        };
        for(int i=0;i<fl;i++){
          fputc(fp[i],of);
        };
        t=(char *)&fs;
        for(int i=0;i<sizeof(uint32_t);i++){
          fputc(t[i],of);
        };
        FILE *f=fopen(fp,"rb");
        char s;
        for(int i=0;i<fs;i++){
          s=fgetc(f);
          fputc(s,of);
        };
        fclose(f);
      }
      else{
        if(strcmp(dir->d_name,".")&&strcmp(dir->d_name,"..")){
          char d_path[257];
          sprintf(d_path,"%s/%s",path,dir->d_name);
          list(d_path,of);
        }
      }
    }
    closedir(d);
}

int main(int argc,char *argv[]){
  FILE *ifile,*ofile;
  ofile=fopen("arcfile","wb");
  list(argv[1],ofile);
  fclose(ofile);
  ifile=fopen("arcfile","rb");
  ofile=fopen(argv[2],"wb");
  pack_initialize();
  pack_file(ifile,ofile);
  fclose(ifile);
  fclose(ofile);
  remove("arcfile");
  return 0;
}
