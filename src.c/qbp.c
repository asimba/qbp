#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#ifndef _WIN32
  #include <unistd.h>
#endif

/***********************************************************************************************************/
//Базовая реализация упаковки/распаковки файла по упрощённому алгоритму LZSS+RC32
/***********************************************************************************************************/

#define LZ_BUF_SIZE 258
#define LZ_CAPACITY 15
#define LZ_MIN_MATCH 3

typedef struct{
  int32_t in;
  int32_t out;
} vocpntr;

uint8_t cbuffer[LZ_CAPACITY+1];
uint8_t lzbuf[LZ_BUF_SIZE*2];
uint8_t vocbuf[0x10000];
int32_t vocarea[0x10000];
vocpntr vocindx[0x10000];
uint32_t frequency[257];
uint16_t buf_size;
uint16_t voclast;
uint16_t vocroot;
uint16_t offset;
uint16_t lenght;
uint32_t low;
uint32_t hlp;
uint32_t range;
uint8_t cflags_count;
uint8_t cbuffer_position;
char *lowp;
char *hlpp;
char eofs;

void pack_initialize(){
  buf_size=cflags_count=offset=\
    lenght=vocroot=cbuffer[0]=eofs=\
      low=hlp=0;
  cbuffer_position=1;
  voclast=0xffff;
  range=0xffffffff;
  lowp=&((char *)&low)[3];
  hlpp=&((char *)&hlp)[0];
  for(uint16_t i=0; i<257; i++) frequency[i]=i;
  for(uint32_t i=0;i<0x10000;i++)
    vocarea[i]=vocindx[i].in=vocindx[i].out=-1;
}

void pack_voc_search(){
  offset=0;
  lenght=1;
  if(buf_size<3) return;
  uint8_t *str=lzbuf;
  uint16_t size=buf_size;
  int32_t cnode=vocindx[*(uint16_t*)str].in;
  char ds=(str[0]^str[1])?0:1;
  while(cnode>=0){
    if(str[lenght]==vocbuf[(uint16_t)(cnode+lenght)]){
      uint16_t tnode=cnode+2;
      uint16_t cl=2;
      while((cl<size)&&(str[cl]==vocbuf[tnode++])) cl++;
      if(cl>=lenght){
        lenght=cl;
        offset=cnode;
      };
      if(lenght==size) break;
    };
    cnode=vocarea[cnode];
    if(ds&&(cnode>=0)) cnode=vocarea[cnode];
  };
  if(lenght>2) offset-=vocroot;
  else lenght=1;
}

void pack_voc_write(){
  vocpntr *indx;
  uint8_t *str=lzbuf;
  uint16_t size=lenght;
  union {uint8_t c[sizeof(uint16_t)];uint16_t i16;} u;
  while(size--){
    u.c[0]=vocbuf[vocroot];
    u.c[1]=vocbuf[(uint16_t)(vocroot+1)];
    vocindx[u.i16].in=vocarea[vocroot];
    vocarea[vocroot]=-1;
    vocbuf[vocroot]=*str++;
    u.c[0]=vocbuf[voclast];
    u.c[1]=vocbuf[vocroot];
    indx=&vocindx[u.i16];
    if(indx->in>=0) vocarea[indx->out]=voclast;
    else indx->in=voclast;
    indx->out=voclast;
    voclast++;
    vocroot++;
  };
}

void rc32_rescale(uint16_t i){
  uint16_t j;
  low+=frequency[i]*range;
  range*=frequency[i+1]-frequency[i];
  for(j=i+1; j<257; j++) frequency[j]++;
  if(frequency[256]>0xffff){
    frequency[0]>>=1;
    for(j=1; j<257; j++){
      frequency[j]>>=1;
      if(frequency[j]<=frequency[j-1])
        frequency[j]=frequency[j-1]+1;
    };
  };
}

int rc32_read(uint8_t *buf,int lenght,FILE *ifile){
  if(!hlp){
    for(uint16_t i=0;i<sizeof(uint32_t);i++){
      hlp<<=8;
      *hlpp=fgetc(ifile);
      if(ferror(ifile)||feof(ifile)){
        hlp=0;
        return -1;
      };
    }
  };
  while(lenght--){
    if(!hlp){
      eofs=1;
      return 1;
    };
    uint16_t i;
    range/=frequency[256];
    uint32_t count=(hlp-low)/range;
    if(count>=frequency[256]) return -1;
    for(i=255; frequency[i]>count; i--) if(!i) break;
    *buf=(uint8_t)i;
    rc32_rescale(i);
    while((range<0x10000)||(hlp<low)){
      if(((low&0xff0000)==0xff0000)&&(range+(uint16_t)low>=0x10000))
        range=0x10000-(uint16_t)low;
      hlp<<=8;
      *hlpp=fgetc(ifile);
      if(ferror(ifile)) return -1;
      if(feof(ifile)) hlp=0;
      if(!hlp) break;
      low<<=8;
      range<<=8;
    };
    buf++;
  };
  return 1;
}

int rc32_write(uint8_t *buf,int lenght,FILE *ofile){
  if(!lenght){
    for(int i=sizeof(uint32_t);i>0;i--){
      fputc(*lowp,ofile);
      if(ferror(ofile)) return -1;
      low<<=8;
    }
    eofs=1;
  }
  else while(lenght--){
    uint16_t i=*buf;
    range/=frequency[256];
    rc32_rescale(i);
    while(range<0x10000){
      if(((low&0xff0000)==0xff0000)&&(range+(uint16_t)low>=0x10000))
        range=0x10000-(uint16_t)low;
      fputc(*lowp,ofile);
      if(ferror(ofile)) return -1;
      low<<=8;
      range<<=8;
    };
    buf++;
  };
  return 1;
}

int pack_file(FILE *ifile,FILE *ofile){
  char eoff=0;
  while(1){
    if(!eoff){
      int r=LZ_BUF_SIZE-buf_size;
      if(r){
        r=fread(lzbuf+buf_size,1,r,ifile);
        if(ferror(ifile)) return -1;
        if(!r&&feof(ifile)) eoff=1;
        else buf_size+=r;
      };
    };
    pack_voc_search();
    if(cflags_count==5){
      cbuffer[0]|=0xa0;
      if(rc32_write(cbuffer,cbuffer_position,ofile)<0) return -1;
      cflags_count=0;
      cbuffer[0]=0;
      cbuffer_position=1;
    };
    cbuffer[0]<<=1;
    if(lenght>=LZ_MIN_MATCH){
      cbuffer[cbuffer_position++]=(uint8_t)(lenght-LZ_MIN_MATCH);
      *(uint16_t*)&cbuffer[cbuffer_position++]=offset;
    }
    else{
      cbuffer[0]|=0x01;
      cbuffer[cbuffer_position]=lzbuf[0];
    };
    cbuffer_position++;
    cflags_count++;
    pack_voc_write();
    memmove(lzbuf,lzbuf+lenght,LZ_BUF_SIZE-lenght);
    buf_size-=lenght;
    if(eoff&&!buf_size){
      if(cflags_count){
        cbuffer[0]<<=(5-cflags_count);
        cbuffer[0]|=(cflags_count<<5);
        if(rc32_write(cbuffer,cbuffer_position,ofile)<0) return -1;
      };
      while(!eofs){
        if(rc32_write(NULL,0,ofile)<0) return -1;
      };
      break;
    }
  };
  return 1;
}

int unpack_file(FILE *ifile, FILE *ofile){
  uint16_t c=0;
  while(1){
    if(buf_size){
      fwrite(lzbuf,1,buf_size,ofile);
      if(ferror(ofile)) return -1;
      buf_size=0;
    }
    else{
      if(eofs) return 1;
      if(cflags_count==0){
        if(rc32_read(cbuffer,1,ifile)<0) return -1;
        cflags_count=cbuffer[0]>>5;
        cbuffer[0]<<=3;
        if(eofs||(cflags_count==0)||(cflags_count==6)||(cflags_count==7))
          return 1;
        cbuffer_position=1;
        c=0;
        uint8_t f=cbuffer[0];
        for(uint8_t cf=0; cf<cflags_count; cf++){
          if(f&0x80) c++;
          else c+=3;
          f<<=1;
        };
        if(rc32_read(&cbuffer[1],c,ifile)<0)
          return -1;
      };
      if(cbuffer[0]&0x80){
        lzbuf[0]=cbuffer[cbuffer_position];
        vocbuf[vocroot++]=cbuffer[cbuffer_position];
        buf_size=1;
      }
      else{
        c=cbuffer[cbuffer_position++]+LZ_MIN_MATCH;
        buf_size=*(uint16_t*)&cbuffer[cbuffer_position++]+vocroot;
        for(uint16_t i=0; i<c; i++){
          lzbuf[i]=vocbuf[buf_size++];
        }
        for(uint16_t i=0; i<c; i++){
          vocbuf[vocroot++]=lzbuf[i];
        };
        buf_size=c;
      };
      cbuffer[0]<<=1;
      cbuffer_position++;
      cflags_count--;
    };
  };
  return 1;
}

/***********************************************************************************************************/

int main(int argc, char *argv[]){
  if((argc<4)||(access(argv[3],F_OK)==0)||\
     ((argv[1][0]!='c')&&(argv[1][0]!='d')))
    goto rpoint00;
  FILE *ifile,*ofile;
  if(!(ifile=fopen(argv[2],"rb"))) goto rpoint00;
  if(!(ofile=fopen(argv[3],"wb"))) goto rpoint01;
  pack_initialize();
  if(argv[1][0]=='c') pack_file(ifile,ofile);
  else unpack_file(ifile,ofile);
  fclose(ofile);
rpoint01:
  fclose(ifile);
rpoint00:
  return 0;
}
