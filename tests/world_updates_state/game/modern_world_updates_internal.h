#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include "world_updates_format.h"
typedef FILE TigFile;
typedef struct TigFileEntry {char path[260];} TigFileEntry;
typedef struct TigFileList {unsigned count;TigFileEntry* entries;} TigFileList;
extern RwPackage* rw_packages;extern unsigned rw_package_count;extern bool rw_active,rw_applying;
void rw_error(const char*,...);void rw_log(const char*,const char*,const char*,const char*);
void rw_sha256(const void*,size_t,uint8_t[32]);bool rw_package_read(const char*,RwPackage*);void rw_package_free(RwPackage*);
bool tig_file_exists(const char*,void*);bool tig_file_is_directory(const char*);bool tig_file_mkdir(const char*);bool tig_file_rmdir(const char*);
bool copy_file_path(const char*,const char*);int tig_file_remove(const char*);int tig_file_rename(const char*,const char*);
void tig_file_list_create(TigFileList*,const char*);void tig_file_list_destroy(TigFileList*);
TigFile* tig_file_fopen(const char*,const char*);
#define tig_file_fclose fclose
#define tig_file_fread fread
#define tig_file_fwrite fwrite
#define tig_file_fgetc fgetc
#define tig_file_fputc fputc
#define tig_file_fprintf fprintf
#define tig_debug_printf(...) ((void)0)
#define tig_file_fseek fseek
int tig_file_filelength(TigFile*);
void rw_state_set_directory(const char*);void rw_state_reset(void);bool rw_state_load(const char*);bool rw_state_save(void);bool rw_state_backup(const char*);bool rw_state_validate_packages(void);int rw_state_outcome(const RwPackage*,const RwOperation*);bool rw_state_record(const RwPackage*,const RwOperation*,unsigned,const char*);bool rw_state_roof_get(const char*,uint64_t,unsigned,uint32_t*);bool rw_state_roof_put(const char*,uint64_t,unsigned,uint32_t);bool rw_save_recover(void);bool rw_save_stage(char[9]);bool rw_save_publish(const char*,const char*,const char*);void rw_save_discard(const char*);
extern int fail_copy;
