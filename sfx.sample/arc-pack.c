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
uint16_t frequency[256][256];
uint16_t fcs[256];
uint16_t buf_size;
uint16_t voclast;
uint16_t vocroot;
uint16_t offset;
uint16_t length;
uint16_t symbol;
uint16_t hs;
uint32_t low;
uint32_t hlp;
uint32_t range;
char *lowp;
char *hlpp;
uint8_t cstate;

void pack_initialize(){
  buf_size=flags=vocroot=*cbuffer=low=hlp=cstate=0;
  voclast=0xfffd;
  range=0xffffffff;
  lowp=&((char *)&low)[3];
  hlpp=&((char *)&hlp)[0];
  uint32_t i,j;
  for(i=0;i<256;i++){
    for(j=0;j<256;j++) frequency[i][j]=1;
    fcs[i]=256;
  };
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
  hs=0x00ff;
}

int rc32_write(uint8_t *buf,int l,FILE *ofile){
  while(l--){
    uint16_t *f=frequency[cstate],fc=fcs[cstate];
    uint32_t i,s=0;
    while((low^(low+range))<0x1000000||range<0x10000){
      fputc(*lowp,ofile);
      if(ferror(ofile)) return -1;
      low<<=8;
      range<<=8;
      if((uint32_t)(range+low)<low) range=~low;
    };
    i=*buf;
    while(i--) s+=*f++;
    low+=s*(range/=fc);
    range*=(*f)++;
    if(++fc==0){
      f=frequency[cstate];
      for(s=0;s<256;s++){
        *f=((*f)>>1)|1;
        fc+=*f++;
      };
    };
    fcs[cstate]=fc;
    cstate=*buf;
    buf++;
  };
  return 1;
}

void pack_file(FILE *ifile,FILE *ofile){
  uint8_t eoff=0,eofs=0;
  uint8_t *cpos=&cbuffer[1];
  vocpntr *indx;
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
          hs^=vocbuf[vocroot];
          hs=(hs<<4)|(hs>>12);
          hashes[voclast]=hs;
          hs^=vocbuf[voclast];
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
    if(buf_size){
      uint16_t symbol,rle,rle_shift=0;
      rle=symbol=vocroot-buf_size;
      while(rle!=vocroot&&vocbuf[++rle]==vocbuf[symbol]);
      rle-=symbol;
      length=LZ_MIN_MATCH;
      if(buf_size>LZ_MIN_MATCH&&rle<buf_size){
        uint16_t cnode=vocindx[hashes[symbol]].in;
        rle_shift=(uint16_t)(vocroot+LZ_BUF_SIZE-buf_size);
        while(cnode!=symbol){
          if(vocbuf[(uint16_t)(symbol+length)]==vocbuf[(uint16_t)(cnode+length)]){
            uint16_t i=symbol,j=cnode;
            while(i!=vocroot&&vocbuf[i]==vocbuf[j++]) i++;
            if((i-=symbol)>=length){
              //while buf_size==LZ_BUF_SIZE: minimal offset > 0x0104;
              if(buf_size<LZ_BUF_SIZE){
                if((uint16_t)(cnode-rle_shift)>0xfeff){
                  cnode=vocarea[cnode];
                  continue;
                };
              };
              offset=cnode;
              if((length=i)==buf_size) break;
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
        for(int i=sizeof(uint32_t);i;i--){
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
