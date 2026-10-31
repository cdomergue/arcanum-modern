#include "world_updates_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
static uint32_t rr(uint32_t x,unsigned n){return (x>>n)|(x<<(32-n));}
void rw_sha256(const void* input,size_t size,uint8_t output[32]){
    static const uint32_t k[64]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    const uint8_t* data=(const uint8_t*)input;size_t blocks=size/64+((size%64)<56?1:2);uint64_t bits=(uint64_t)size*8;
    for(size_t block=0;block<blocks;block++){
        uint8_t bytes[64]={0};size_t offset=block*64;
        for(unsigned i=0;i<64;i++){size_t at=offset+i;if(at<size)bytes[i]=data[at];else if(at==size)bytes[i]=0x80;}
        if(block+1==blocks)for(unsigned i=0;i<8;i++)bytes[63-i]=(uint8_t)(bits>>(i*8));
        uint32_t w[64];for(unsigned i=0;i<16;i++)w[i]=((uint32_t)bytes[i*4]<<24)|((uint32_t)bytes[i*4+1]<<16)|((uint32_t)bytes[i*4+2]<<8)|bytes[i*4+3];
        for(unsigned i=16;i<64;i++){uint32_t a=w[i-15],b=w[i-2];w[i]=w[i-16]+(rr(a,7)^rr(a,18)^(a>>3))+w[i-7]+(rr(b,17)^rr(b,19)^(b>>10));}
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],z=h[7];
        for(unsigned i=0;i<64;i++){uint32_t t1=z+(rr(e,6)^rr(e,11)^rr(e,25))+((e&f)^((~e)&g))+k[i]+w[i],t2=(rr(a,2)^rr(a,13)^rr(a,22))+((a&b)^(a&c)^(b&c));z=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=z;
    }
    for(unsigned i=0;i<8;i++)for(unsigned j=0;j<4;j++)output[i*4+j]=(uint8_t)(h[i]>>(24-j*8));
}
typedef struct Reader {const uint8_t* data;size_t size,pos;char* error;size_t cap;bool valid;} Reader;
static bool bad(Reader* r,const char* text){if(r->valid && r->cap)snprintf(r->error,r->cap,"%s at byte %zu",text,r->pos);r->valid=false;return false;}
static bool take(Reader* r,void* out,size_t count){if(!r->valid || count>r->size-r->pos)return bad(r,"Truncated package");if(count)memcpy(out,r->data+r->pos,count);r->pos+=count;return true;}
static uint32_t u32(Reader* r){uint8_t b[4]={0};take(r,b,4);return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);}
static uint64_t u64(Reader* r){uint64_t a=u32(r),b=u32(r);return a|(b<<32);}
static bool str(Reader* r,char* out,size_t cap){unsigned n=u32(r);if(n>=cap)return bad(r,"String too long");if(!take(r,out,n))return false;out[n]=0;if(memchr(out,0,n))return bad(r,"Embedded null in string");return true;}
static bool identifier(const char* value){if(!isalnum((unsigned char)*value))return false;for(;*value;value++)if(!(isalnum((unsigned char)*value)||*value=='_'||*value=='-'||*value=='.'))return false;return true;}
bool rw_codec_path(const char* path){if(!*path || *path=='/' || strchr(path,'\\') || strchr(path,':'))return false;const char* part=path;for(const char* p=path;;p++){unsigned char c=(unsigned char)*p;if(c && (c<32 || c>=127 || (c>='A' && c<='Z')))return false;if(c=='/' || !c){size_t n=(size_t)(p-part);if(!n || (n==1 && *part=='.') || (n==2 && part[0]=='.' && part[1]=='.'))return false;part=p+1;if(!c)break;}}return true;}
static bool file_bytes(const char* path,uint8_t** out,size_t* size){FILE* f=fopen(path,"rb");if(!f)return false;if(fseek(f,0,SEEK_END)){fclose(f);return false;}long n=ftell(f);if(n<0 || (unsigned long)n>RW_MAX_PACKAGE_BYTES || fseek(f,0,SEEK_SET)){fclose(f);return false;}uint8_t* b=(uint8_t*)malloc(n?n:1);if(!b){fclose(f);return false;}bool ok=fread(b,1,(size_t)n,f)==(size_t)n;ok=fclose(f)==0 && ok;if(!ok){free(b);return false;}*out=b;*size=(size_t)n;return true;}
void rw_codec_free(RwPackage* p){free(p->dependencies);free(p->resources);free(p->operations);memset(p,0,sizeof(*p));}
bool rw_codec_read(const char* directory,RwPackage* p,char* error,size_t cap){
    memset(p,0,sizeof(*p));if(cap)error[0]=0;char path[1024];uint8_t* bytes=NULL;size_t size=0;
    if(strlen(directory)>=sizeof(p->directory) || snprintf(path,sizeof(path),"%s/runtime.rwp",directory)>=(int)sizeof(path) || !file_bytes(path,&bytes,&size)){if(cap)snprintf(error,cap,"Cannot read bounded runtime.rwp in %s",directory);return false;}
    Reader r={bytes,size,0,error,cap,true};char magic[8]={0};take(&r,magic,8);if(memcmp(magic,"RWPKG01",8) || u32(&r)!=RW_ABI)bad(&r,"Unsupported package ABI");
    str(&r,p->id,sizeof(p->id));str(&r,p->version,sizeof(p->version));p->revision=u32(&r);p->min_abi=u32(&r);p->max_abi=u32(&r);str(&r,p->module,sizeof(p->module));
    if(!identifier(p->id) || !identifier(p->module) || !p->revision || p->min_abi>RW_ABI || p->max_abi<RW_ABI)bad(&r,"Invalid package identity or compatibility");
    p->dependency_count=u32(&r);if(p->dependency_count>64)bad(&r,"Too many dependencies");
    if(r.valid)p->dependencies=(RwDependency*)calloc(p->dependency_count?p->dependency_count:1,sizeof(RwDependency));if(r.valid && !p->dependencies)bad(&r,"Out of memory");
    for(unsigned i=0;r.valid && i<p->dependency_count;i++){RwDependency* d=&p->dependencies[i];str(&r,d->id,sizeof(d->id));d->revision=u32(&r);if(!identifier(d->id)||!d->revision||!strcmp(d->id,p->id))bad(&r,"Invalid dependency");for(unsigned j=0;j<i;j++)if(!strcmp(d->id,p->dependencies[j].id))bad(&r,"Duplicate dependency");}
    p->resource_count=u32(&r);if(p->resource_count>RW_MAX_RESOURCES)bad(&r,"Too many resources");if(r.valid)p->resources=(RwResource*)calloc(p->resource_count?p->resource_count:1,sizeof(RwResource));if(r.valid&&!p->resources)bad(&r,"Out of memory");
    for(unsigned i=0;r.valid&&i<p->resource_count;i++){RwResource* a=&p->resources[i];str(&r,a->path,sizeof(a->path));take(&r,a->digest,32);if(!rw_codec_path(a->path) || (strncmp(a->path,"objects/",8)&&strncmp(a->path,"proto/",6)&&strncmp(a->path,"scr/",4)&&strncmp(a->path,"dlg/",4)))bad(&r,"Invalid package resource path");for(unsigned j=0;j<i;j++)if(!strcmp(a->path,p->resources[j].path))bad(&r,"Duplicate resource");}
    p->operation_count=u32(&r);if(p->operation_count>RW_MAX_OPERATIONS)bad(&r,"Too many operations");if(r.valid)p->operations=(RwOperation*)calloc(p->operation_count?p->operation_count:1,sizeof(RwOperation));if(r.valid&&!p->operations)bad(&r,"Out of memory");
    for(unsigned i=0;r.valid&&i<p->operation_count;i++){
        size_t start=r.pos;RwOperation* o=&p->operations[i];str(&r,o->id,sizeof(o->id));o->revision=u32(&r);o->kind=u32(&r);o->policy=u32(&r);str(&r,o->map,sizeof(o->map));o->sector=u64(&r);o->selector=u32(&r);str(&r,o->guid,sizeof(o->guid));o->ordinal=u32(&r);o->prototype=u32(&r);o->type=u32(&r);str(&r,o->base_path,sizeof(o->base_path));take(&r,o->base_hash,32);str(&r,o->field,sizeof(o->field));o->index=u32(&r);o->mask=u64(&r);for(unsigned j=0;j<3;j++)o->before[j]=u64(&r);for(unsigned j=0;j<3;j++)o->after[j]=u64(&r);str(&r,o->payload_path,sizeof(o->payload_path));
        if(!identifier(o->id) || !o->revision || o->revision>p->revision || o->kind<RW_ADD || o->kind>RW_GLOBAL || o->policy>1 || o->selector>2 || (o->kind!=RW_GLOBAL && (!identifier(o->map) || (o->sector&0x3ffffff)>=2000 || (o->sector>>26)>=2000)))bad(&r,"Invalid operation metadata");
        if(i && o->revision<p->operations[i-1].revision)bad(&r,"Operation history out of order");
        if(o->base_path[0] && !rw_codec_path(o->base_path))bad(&r,"Invalid reference path");
        if(o->kind==RW_ADD){bool found=false;for(unsigned j=0;j<p->resource_count;j++)if(!strcmp(o->payload_path,p->resources[j].path))found=true;if(!found || strncmp(o->payload_path,"objects/",8))bad(&r,"Missing object payload");}
        else if(o->payload_path[0])bad(&r,"Unexpected object payload");
        for(unsigned j=0;j<i;j++)if(!strcmp(o->id,p->operations[j].id))bad(&r,"Duplicate operation ID");
        if(r.valid)rw_sha256(bytes+start,r.pos-start,o->digest);
    }
    if(r.valid && r.pos!=r.size)bad(&r,"Trailing package data");
    if(r.valid){rw_sha256(bytes,size,p->digest);strcpy(p->directory,directory);}
    free(bytes);
    for(unsigned i=0;r.valid && i<p->resource_count;i++){uint8_t* data=NULL;size_t n=0;uint8_t digest[32];snprintf(path,sizeof(path),"%s/%s",directory,p->resources[i].path);if(!file_bytes(path,&data,&n)){bad(&r,"Missing or oversized resource");break;}rw_sha256(data,n,digest);free(data);if(memcmp(digest,p->resources[i].digest,32))bad(&r,"Resource digest mismatch");}
    if(!r.valid){rw_codec_free(p);return false;}return true;
}
