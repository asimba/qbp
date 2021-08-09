#include <dirent.h>
#include <stdio.h>
#include <string.h>
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
  buf_size=flags=vocroot=*cbuffer=low=hlp=0;
  voclast=range=0xffffffff;
  lowp=&((char *)&low)[3];
  hlpp=&((char *)&hlp)[0];
  fc=&frequency[256];
  uint32_t i;
  for(i=0;i<257;i++) frequency[i]=i;
  for(i=0;i<0x10000;i++){
    vocbuf[i]=(uint8_t)i;
    vocbuf[i]=0xff;
    vocindx[i].in=1;
    vocindx[i].out=0;
    vocarea[i]=(uint16_t)(i+1);
  };
  vocindx[0xffff].in=0;
  vocindx[0xffff].out=0xfffe;
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

int rc32_write(uint8_t *buf,int l,FILE *ofile){
  while(l--){
    while(range<0x10000){
      if(((low&0xff0000)==0xff0000)&&(range+(uint16_t)low>=0x10000))
        range=0x10000-(uint16_t)low;
      fputc(*lowp,ofile);
      if(ferror(ofile)) return -1;
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
  union {uint8_t c[sizeof(uint16_t)];uint16_t i16;} u;
  uint16_t i,rle,rle_shift,cnode;
  char eoff=0,eofs=0;
  vocpntr *indx;
  uint8_t *cpos=&cbuffer[1];
  flags=8;
  while(1){
    if(!eoff){
      if(LZ_BUF_SIZE-buf_size){
        symbol=fgetc(ifile);
        if(ferror(ifile)) break;
        if(feof(ifile)) eoff=1;
        else{
          u.c[0]=vocbuf[vocroot];
          u.c[1]=vocbuf[(uint16_t)(vocroot+1)];
          if(vocarea[vocroot]==vocroot){
            vocindx[u.i16].in=1;
            vocindx[u.i16].out=0;
          }
          else vocindx[u.i16].in=vocarea[vocroot];
          vocarea[vocroot]=vocroot;
          vocbuf[vocroot]=symbol;
          u.c[0]=vocbuf[voclast];
          u.c[1]=vocbuf[vocroot];
          indx=&vocindx[u.i16];
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
    symbol=vocroot-buf_size;
    if(buf_size){
      rle=1;
      while(rle<buf_size){
        if(vocbuf[symbol]==vocbuf[(uint16_t)(symbol+rle)]) rle++;
        else break;
      };
      lenght=LZ_MIN_MATCH;
      if(buf_size>LZ_MIN_MATCH){
        u.c[0]=vocbuf[symbol];
        u.c[1]=vocbuf[(uint16_t)(symbol+1)];
        cnode=vocindx[u.i16].in;
        rle_shift=(uint16_t)(vocroot+LZ_BUF_SIZE-buf_size);
        while(cnode!=symbol){
          if(vocbuf[(uint16_t)(symbol+lenght)]==vocbuf[(uint16_t)(cnode+lenght)]){
            i=2;
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
