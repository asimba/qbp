#ifndef _WIN32
  #include <unistd.h>
#endif
#include "lzss.h"

int main(int argc, char *argv[]){
  if((argc<4)||(access(argv[3],F_OK)==0)||\
     ((argv[1][0]!='c')&&(argv[1][0]!='d')))
    goto rpoint00;
  lzss_data lzssd;
  lzss_initialize(&lzssd);
  if(!lzssd.lzbuf) goto rpoint00;
  FILE *istream=NULL;
  FILE *ostream=NULL;
  int l=0;
  if(!(istream=fopen(argv[2],"rb"))) goto rpoint01;
  if(!(ostream=fopen(argv[3],"wb"))) goto rpoint02;
  char *buffer=(char *)calloc(32768,sizeof(char));
  if(buffer){
    if(argv[1][0]=='c'){
      lzssd.pack.file=ostream;
      while(1){
        l=fread(buffer,sizeof(char),32768,istream);
        if(ferror(istream)) break;
        if(!l&&feof(istream)){
          lzss_write(&lzssd,NULL,0);
          break;
        }
        else if(lzss_write(&lzssd,buffer,l)<0) break;
      };
    }
    else{
      lzssd.pack.file=istream;
      while(1){
        l=lzss_read(&lzssd,buffer,32768);
        if(l<=0) break;
        if(fwrite(buffer,sizeof(char),l,ostream)<0) break;
        if(lzss_is_eof(&lzssd)) break;
      };
    };
    free(buffer);
  };
  fclose(ostream);
rpoint02:
  fclose(istream);
rpoint01:
  lzss_release(&lzssd);
rpoint00:
  return 0;
}
