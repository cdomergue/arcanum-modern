#ifndef ARCANUM_GAME_WRITTEN_H_
#define ARCANUM_GAME_WRITTEN_H_

// NOTE: Originally `Written_Type` in `Item.h` (as noted in
// `oemes/written.mes`).
typedef enum WrittenType {
    WRITTEN_TYPE_BOOK,
    WRITTEN_TYPE_NOTE,
    WRITTEN_TYPE_NEWSPAPER,
    WRITTEN_TYPE_TELEGRAM,
    WRITTEN_TYPE_IMAGE,
    WRITTEN_TYPE_SCHEMATIC,
    WRITTEN_TYPE_PLAQUE,
    WRITTEN_TYPE_COUNT,
} WrittenType;

#endif /* ARCANUM_GAME_WRITTEN_H_ */
