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

typedef union{
  struct{
    uint16_t in;
    uint16_t out;
  };
  uint32_t val;
} vocpntr;

uint8_t flags;
uint8_t scntx;
uint8_t cbuffer[LZ_CAPACITY+1];
uint8_t cntxs[LZ_CAPACITY+1];
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
uint32_t low;
uint32_t hlp;
uint32_t range;
char *lowp;
char *hlpp;

void pack_initialize(){
  cntxs[0]=flags=buf_size=vocroot=*cbuffer=low=hlp=0;
  voclast=0xfffc;
  range=0xffffffff;
  lowp=&((char *)&low)[3];
  hlpp=&((char *)&hlp)[0];
  for(int i=0;i<256;i++){
    for(int j=0;j<256;j++) frequency[i][j]=1;
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

uint32_t rc32_putc(uint32_t c,FILE *ofile,uint8_t cntx){
  while((low^(low+range))<0x1000000||range<0x10000){
    fputc(*lowp,ofile);
    if(ferror(ofile)) return 1;
    low<<=8;
    range<<=8;
    if((uint32_t)(range+low)<low) range=~low;
  };
  uint16_t *f=frequency[cntx],fc=fcs[cntx];
  register uint64_t s=0;
  while(c>3) s+=*(uint64_t *)f,f+=4,c-=4;
  if(c>>1) s+=*(uint32_t *)f,f+=2;
  if(c&1) s+=*f++;
  s+=s>>32;
  s+=s>>16;
  low+=((uint16_t)s)*(range/=fc);
  range*=(*f)++;
  if(!++fc){
    f=frequency[cntx];
    for(s=0;s<256;s++) fc+=(*f=((*f)>>1)|(*f&1)),f++;
  };
  fcs[cntx]=fc;
  return 0;
}

void pack_file(FILE *ifile,FILE *ofile){
  uint8_t *cpos=&cbuffer[1],eoff=0,cntx=1;
  flags=8;
  for(;;){
    if(!eoff&&buf_size!=LZ_BUF_SIZE){
      vocbuf[vocroot]=fgetc(ifile);
      if(ferror(ifile)) break;
      if(feof(ifile)) eoff=1;
      else{
        if(vocarea[vocroot]==vocroot) vocindx[hashes[vocroot]].val=1;
        else vocindx[hashes[vocroot]].in=vocarea[vocroot];
        vocarea[vocroot]=vocroot;
        uint16_t hs=hashes[voclast]^vocbuf[voclast]^vocbuf[vocroot++];
        vocpntr *indx=&vocindx[(hashes[++voclast]=(hs<<4)|(hs>>12))];
        if(indx->val==1) indx->in=voclast;
        else vocarea[indx->out]=voclast;
        indx->out=voclast;
        buf_size++;
      };
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
        cntxs[cntx++]=1;
        cntxs[cntx++]=2;
        cntxs[cntx++]=3;
        *cpos++=--i;
        *(uint16_t*)cpos++=offset;
        buf_size-=length;
      }
      else{
        cntxs[cntx++]=scntx;
        scntx=vocbuf[symbol];
        *cbuffer|=1;
        *cpos=vocbuf[symbol];
        buf_size--;
      };
    }
    else{
      cntxs[cntx++]=1;
      cntxs[cntx++]=2;
      cntxs[cntx++]=3;
      length=0;
      cpos++;
      *(uint16_t*)cpos++=0x0100;
    };
    cpos++;
    if(--flags&&length) continue;
    *cbuffer<<=flags;
    for(int i=0;i<cpos-cbuffer;i++)
      if(rc32_putc(cbuffer[i],ofile,cntxs[i])) break;
    if(!length){
      for(int i=4;i;i--){
        fputc(*lowp,ofile);
        if(ferror(ofile)) return;
        low<<=8;
      };
      break;
    };
    cpos=&cbuffer[1];
    cntx=1;
    flags=8;
  };
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
