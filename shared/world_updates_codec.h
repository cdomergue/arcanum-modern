#ifndef RENAISSANCE_WORLD_UPDATES_CODEC_H
#define RENAISSANCE_WORLD_UPDATES_CODEC_H
#include "world_updates_format.h"
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
void rw_sha256(const void* data,size_t size,uint8_t digest[32]);
bool rw_codec_read(const char* directory,RwPackage* out,char* error,size_t error_size);
void rw_codec_free(RwPackage* package);
bool rw_codec_path(const char* path);
#ifdef __cplusplus
}
#endif
#endif
