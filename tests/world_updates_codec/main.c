#include "world_updates_codec.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int check(const char* input, size_t count, const char* expected) {
    unsigned char hash[32]; char hex[65];
    rw_sha256(input,count,hash);
    for(unsigned i=0;i<32;i++)sprintf(hex+i*2,"%02x",hash[i]);
    if(strcmp(hex,expected)){fprintf(stderr,"SHA mismatch: %s\n",hex);return 1;}return 0;
}
int main(int argc,char** argv) {
    int failures=check("",0,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    failures+=check("abc",3,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const char* long_vector="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    failures+=check(long_vector,strlen(long_vector),"248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    char* million=malloc(1000000);if(!million)return 1;memset(million,'a',1000000);
    failures+=check(million,1000000,"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");free(million);
    if(rw_codec_path("../bad") || rw_codec_path("objects/A.mob") || rw_codec_path("a//b") || !rw_codec_path("objects/new.mob"))failures++;
    if(argc>1){RwPackage package;char error[512];if(!rw_codec_read(argv[1],&package,error,sizeof(error))){fprintf(stderr,"%s\n",error);return 2;}printf("%s: %u operations\n",package.id,package.operation_count);rw_codec_free(&package);}
    return failures?1:0;
}
