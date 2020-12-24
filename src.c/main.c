#ifndef _WIN32
  #include <unistd.h>
#endif
#include "lzss.h"

int main(int argc, char *argv[]){
  if(argc>3){
    if(access(argv[3],F_OK)==0) return -1;
    char *buffer=(char *)calloc(32768,sizeof(char));
    if(!buffer) return -1;
    lzss_data lzssd;
    lzss_initialize(&lzssd);
    if(!lzssd.lzbuf){
      free(buffer);
      return -1;
    };
    FILE *istream=NULL;
    FILE *ostream=NULL;
    if(argv[1][0]=='c'){
      if((istream=fopen(argv[2],"rb"))){
        if((ostream=fopen(argv[3],"wb"))){
          int l=0;
          while(1){
            l=fread(buffer,sizeof(char),32768,istream);
            if(l<0) break;
            if(!l){
              lzss_write(&lzssd,ostream,NULL,0);
              break;
            }
            else if(lzss_write(&lzssd,ostream,buffer,l)<0) break;
          };
          fclose(ostream);
        };
        fclose(istream);
      };
    }
    else{
      if(argv[1][0]=='d'){
        if((istream=fopen(argv[2],"rb"))){
          if((ostream=fopen(argv[3],"wb"))){
            int l=0;
            while(1){
              l=lzss_read(&lzssd,istream,buffer,32768);
              if(l<=0) break;
              if(fwrite(buffer,sizeof(char),l,ostream)<0) break;
              if(lzss_is_eof(&lzssd)) break;
            };
            fclose(ostream);
          };
          fclose(istream);
        };
      }
    };
    free(buffer);
    lzss_release(&lzssd);
  };
  return 0;
}
