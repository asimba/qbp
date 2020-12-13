#include "lzss.h"

int32_t read_operator(void *file, char *buffer){
  buffer[0]=fgetc((FILE*)file);
  if(ferror((FILE*)file)) return -1;
  if(feof((FILE*)file)){
    buffer[0]=0;
    return 0;
  }
  return 1;
}

int32_t write_operator(void *file, char *buffer){
  fputc(*buffer,(FILE*)file);
  if(ferror((FILE*)file)) return -1;
  return 1;
}

int32_t is_eof_operator(void *file){
  return feof((FILE*)file);
}

int main(int argc, char *argv[]){
  char *buffer=(char *)calloc(32768,sizeof(char));
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
                l=fread(buffer,sizeof(char),32768,istream);
                if(l<0) break;
                if(l==0){
                  while(pack.is_eof()==0){
                    if(pack.write(ostream,NULL,0)<0) break;
                  };
                  break;
                };
                if(pack.write(ostream,buffer,l)<0) break;
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
                  l=pack.read(istream,buffer,32768);
                  if(l<=0) break;
                  if(fwrite(buffer,sizeof(char),l,ostream)<0) break;
                  if(pack.is_eof()) break;
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
