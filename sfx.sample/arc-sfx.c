#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>

#define LZ_BUF_SIZE 259
#define LZ_CAPACITY 24
#define LZ_MIN_MATCH 3

uint8_t flags;
uint8_t cbuffer[LZ_CAPACITY+1];
uint8_t cntxs[LZ_CAPACITY+2];
uint8_t vocbuf[0x10000];
uint16_t frequency[256][256];
uint16_t fcs[256];
uint16_t buf_size;
uint16_t vocroot;
uint16_t offset;
uint16_t length;
uint16_t symbol;
uint32_t low;
uint32_t hlp;
uint32_t range;
char *lowp;
char *hlpp;
uint8_t *cpos;
uint8_t rle_flag;
uint8_t scntx;

uint8_t rc32_getc(uint8_t *c,FILE *ifile,uint8_t cntx){
  uint16_t *f=frequency[cntx],fc=fcs[cntx];
  uint32_t s=0,i;
  while(hlp<low||(low^(low+range))<0x1000000||range<0x10000){
    hlp<<=8;
    *hlpp=fgetc(ifile);
    if(ferror(ifile)) return 1;
    if(feof(ifile)) return 0;
    low<<=8;
    range<<=8;
    if((uint32_t)(range+low)<low) range=~low;
  };
  if((i=(hlp-low)/(range/=fc))<fc){
    while((s+=*f)<=i) f++;
    low+=(s-*f)*range;
    *c=(uint8_t)(f-frequency[cntx]);
    range*=(*f)++;
    if(!++fc){
      f=frequency[cntx];
      for(s=0;s<256;s++) fc+=(*f=((*f)>>1)|(*f&1)),f++;
    };
    fcs[cntx]=fc;
    return 0;
  }
  else return 1;
}

uint8_t unpack_file(FILE *ifile){
  for(;;){
    if(length){
      if(rle_flag==0) symbol=vocbuf[offset++];
      vocbuf[vocroot++]=symbol;
      length--;
      return 0;
    }
    else{
      if(flags==0){
        cpos=cbuffer;
        if(rc32_getc(cpos++,ifile,0)) return 1;
        for(uint8_t c=~*cbuffer;c;length++) c&=c-1;
        uint8_t cflags=*cbuffer;
        uint8_t cntx=8+(length<<1);
        for(uint8_t c=0;c<cntx;){
          if(cflags&0x80) cntxs[c++]=4;
          else{
            *(uint32_t*)(cntxs+c)=0x00030201;
            c+=3;
          };
          cflags<<=1;
        };
        for(uint8_t c=0;c<cntx;c++){
          if(cntxs[c]==4){
            if(rc32_getc(cpos,ifile,scntx)) return;
            scntx=*cpos++;
          }
          else{
            if(rc32_getc(cpos++,ifile,cntxs[c])) return;
          };
        };
        cpos=&cbuffer[1];
        flags=8;
      };
      length=rle_flag=1;
      if(*cbuffer&0x80) symbol=*cpos;
      else{
        length=LZ_MIN_MATCH+1+*cpos++;
        if((offset=*(uint16_t*)cpos++)<0x0100) symbol=(uint8_t)(offset);
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
  return 0;
}

void rmkdir(char *path,int depth){
  for(int i=depth;i<strlen(path);i++){
    if(path[i]=='/'){
      path[i]=0;
      if(access(path,0)!=0){
        mkdir(path);
        if(access(path,0)!=0) return;
      };
      path[i]='/';
      depth=i+1;
      rmkdir(path,depth);
      break;
    };
  };
  return;
}

void read_value(void *p,uint16_t s,FILE *f){
  for(uint16_t i=0;i<s;i++) ((char *)p)[i]=fgetc(f);
}

uint8_t read_packed_value(void *p,uint16_t s,FILE *f){
  for(uint16_t i=0;i<s;i++){
    if(unpack_file(f)) return 1;
    ((char *)p)[i]=(char)symbol;
  };
  return 0;
}

void unarc(char *out,char *fn){
  FILE *f=fopen(fn,"rb"),*o;
  uint32_t fl=0,i;
  char *t=(char *)&fl,fp[257];
  fseek(f,60,SEEK_SET);
  read_value(&range,sizeof(uint32_t),f);
  fseek(f,range+6,SEEK_SET);
  read_value(&length,sizeof(uint16_t),f);
  fseek(f,0x100,SEEK_CUR);
  while(length--){
    read_value(&hlp,sizeof(uint32_t),f);
    read_value(&low,sizeof(uint32_t),f);
    if(low>fl){
      fl=low;
      range=low+hlp;
    }
    fseek(f,32,SEEK_CUR);
  };
  fseek(f,range,SEEK_SET);
  if(feof(f)) return;
  buf_size=flags=vocroot=low=hlp=length=rle_flag=0;
  offset=range=0xffffffff;
  scntx=0xff;
  lowp=&((char *)&low)[3];
  hlpp=&((char *)&hlp)[0];
  for(i=0;i<256;i++){
    for(int j=0;j<256;j++) frequency[i][j]=1;
    fcs[i]=256;
  };
  for(i=0;i<0x10000;i++) vocbuf[i]=0xff;
  for(i=0;i<sizeof(uint32_t);i++){
    hlp<<=8;
    *hlpp=fgetc(f);
    if(ferror(f)||feof(f)) return;
  }
  for(;;){
    if(read_packed_value(t,sizeof(uint32_t),f)) goto close_fp;
    if(feof(f)) break;
    for(i=0;i<strlen(out);i++) fp[i]=out[i];
    fp[i]='/';
    if(read_packed_value(&fp[strlen(out)+1],fl,f)) goto close_fp;
    fp[fl+strlen(out)+1]=0;
    if(read_packed_value(t,sizeof(uint32_t),f)) goto close_fp;
    rmkdir(fp,0);
    o=fopen(fp,"wb");
    for(i=0;i<fl;i++){
      if(unpack_file(f)) goto close_fp;
      if(o) fputc((char)symbol,o);
    };
    if(o) fclose(o);
  };
close_fp:
  fclose(f);
}

int main(int argc,char *argv[]){
  unarc("./",argv[0]);
  return 0;
}
