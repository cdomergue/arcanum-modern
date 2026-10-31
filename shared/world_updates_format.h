#ifndef RENAISSANCE_WORLD_UPDATES_FORMAT_H
#define RENAISSANCE_WORLD_UPDATES_FORMAT_H
#include <stdint.h>
#include <stddef.h>
#define RW_ABI 1u
#define RW_MAX_PACKAGES 128u
#define RW_MAX_OPERATIONS 8192u
#define RW_MAX_RESOURCES 4096u
#define RW_MAX_PACKAGE_BYTES (64u*1024u*1024u)
#define RW_ID_SIZE 64
#define RW_PATH_SIZE 260
#define RW_HASH_SIZE 32
/* On disk: little endian integers; strings are u32 byte length + UTF-8 bytes,
 * without terminators. Never serialize these in-memory structs directly. */
enum RwKind { RW_ADD=1,RW_MOVE=2,RW_FIELD=3,RW_FLAGS=4,RW_GROUND=5,RW_ROOF=6,RW_BLOCK=7,RW_HIDE=8,RW_GLOBAL=9 };
enum RwSelector { RW_NONE=0,RW_GUID=1,RW_STATIC=2 };
enum RwPolicy { RW_CONDITIONAL=0,RW_AUTHORITATIVE=1 };
enum RwOutcome { RW_PENDING=0,RW_APPLIED=1,RW_CONFLICT=2,RW_FAILED=3 };
typedef struct RwDependency {char id[RW_ID_SIZE];uint32_t revision;} RwDependency;
typedef struct RwResource {char path[RW_PATH_SIZE];uint8_t digest[RW_HASH_SIZE];} RwResource;
typedef struct RwOperation {
    char id[RW_ID_SIZE];uint32_t revision,kind,policy;
    char map[RW_ID_SIZE];uint64_t sector;
    uint32_t selector;char guid[37];uint32_t ordinal,prototype,type;
    char base_path[RW_PATH_SIZE];uint8_t base_hash[RW_HASH_SIZE];
    char field[RW_ID_SIZE];uint32_t index;uint64_t mask,before[3],after[3];
    char payload_path[RW_PATH_SIZE];uint8_t digest[RW_HASH_SIZE];
} RwOperation;
typedef struct RwPackage {
    char id[RW_ID_SIZE],version[32],module[RW_ID_SIZE],directory[RW_PATH_SIZE];
    uint32_t revision,min_abi,max_abi,dependency_count,resource_count,operation_count;
    RwDependency* dependencies;RwResource* resources;RwOperation* operations;
    uint8_t digest[RW_HASH_SIZE];
} RwPackage;
#endif
