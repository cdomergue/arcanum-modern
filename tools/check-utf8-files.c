#undef NDEBUG
#include <assert.h>
#include <tig/compat.h>

int main(int argc, char** argv)
{
    assert(argc == 2);
    assert(SDL_CreateDirectory(argv[1]));
    char path[1024];
    SDL_snprintf(path, sizeof(path), "%s/Vormantown (Fran\xc3\xa7" "ais) \xf0\x9f\x8e\xb2.dat", argv[1]);
    FILE* file = compat_fopen(path, "wb");
    assert(file && fwrite("test", 1, 4, file) == 4);
    assert(fclose(file) == 0);
    size_t size;
    char* bytes = SDL_LoadFile(path, &size);
    assert(bytes && size == 4 && memcmp(bytes, "test", 4) == 0);
    SDL_free(bytes);
    file = compat_fopen(path, "rb");
    char buffer[4];
    assert(file && fread(buffer, 1, 4, file) == 4 && memcmp(buffer, "test", 4) == 0);
    assert(fclose(file) == 0);
    assert(SDL_RemovePath(path));
    puts("PASS: engine and SDL open the same UTF-8 filename, including accents and non-BMP characters");
    return 0;
}
