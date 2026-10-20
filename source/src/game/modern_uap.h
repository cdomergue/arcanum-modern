#ifndef ARCANUM_MODERN_UAP_H
#define ARCANUM_MODERN_UAP_H
#include <stdbool.h>
#include <tig/file.h>
bool modern_uap_ready(void);
bool modern_uap_load(void);
TigFile* modern_uap_original_sector(const char* path);
bool modern_uap_module_load(const char* module);
void modern_uap_module_unload(void);
#endif
