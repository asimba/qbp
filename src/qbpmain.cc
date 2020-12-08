#include "lzss.h"

int32_t read_operator(void *file, char *buffer, int32_t lenght){
    int32_t ret=0;
    if((ret=fread(buffer,sizeof(uint8_t),_RC32_BUFFER_SIZE,(FILE*)file))==0){
      if(ferror((FILE*)file)) return -1;
    };
    return ret;
}

int32_t write_operator(void *file, char *buffer, int32_t lenght){
    int32_t ret=0;
    if(lenght&&((ret=fwrite(buffer,sizeof(uint8_t),lenght,(FILE*)file))==0)) return -1;
    return ret;
}

int32_t is_eof_operator(void *file){
  return feof((FILE*)file);
}

int main(int argc, char *argv[]){
  char *buffer=(char *)calloc(_RC32_BUFFER_SIZE,sizeof(char));
  if(buffer){
    lzss pack;
    pack.set_operators(read_operator,write_operator,is_eof_operator);
    if(argc>3){
      FILE *istream=NULL;
      FILE *ostream=NULL;
      if(argv[1][0]=='c'){
        if(access(argv[3],F_OK)!=0){
          if((istream=fopen64(argv[2],"rb"))!=NULL){
            if((ostream=fopen64(argv[3],"wb"))!=NULL){
              int l=0;
              while(1){
                l=fread(buffer,sizeof(char),_RC32_BUFFER_SIZE,istream);
                if(l<0) break;
                if(l==0){
                  pack.finalize=true;
                  while(pack.is_eof(ostream)==0){
                    if(pack.lzss_write(ostream,NULL,0)<0) break;
                  };
                  break;
                };
                if(pack.lzss_write(ostream,buffer,l)<0) break;
              };
              fclose(ostream);
            };
            fclose(istream);
          };
        };
      }
      else{
        if(argv[1][0]=='d'){
          if(access(argv[3],F_OK)!=0){
            if((istream=fopen64(argv[2],"rb"))!=NULL){
              if((ostream=fopen64(argv[3],"wb"))!=NULL){
                int l=0;
                while(1){
                  l=pack.lzss_read(istream,buffer,_RC32_BUFFER_SIZE);
                  if(l<=0) break;
                  if(fwrite(buffer,sizeof(char),l,ostream)<0) break;
                  if(pack.is_eof(istream)) break;
                };
                fclose(ostream);
              };
              fclose(istream);
            };
          };
        };
      };
    };
    free(buffer);
  };
  return 0;
}
