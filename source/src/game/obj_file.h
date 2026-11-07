#ifndef ARCANUM_GAME_OBJ_FILE_H_
#define ARCANUM_GAME_OBJ_FILE_H_

#include "game/context.h"

bool objf_solitary_write(int64_t handle, const char* dir, const char* ext);
bool objf_solitary_read(int64_t* handle_ptr, const char* path);
void objf_solitary_delete(int64_t handle, const char* dir, const char* ext);
bool objf_write(const void* buffer, size_t size, TigFile* stream);
bool objf_read(void* buffer, size_t size, TigFile* stream);

#endif /* ARCANUM_GAME_OBJ_FILE_H_ */
