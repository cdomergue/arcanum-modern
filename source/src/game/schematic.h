#ifndef ARCANUM_GAME_SCHEMATIC_H_
#define ARCANUM_GAME_SCHEMATIC_H_

typedef struct SchematicInfo {
    int name;
    int description;
    int art_num;
    int item1[3];
    int item2[3];
    int prod[3];
    int qty;
} SchematicInfo;

#endif /* ARCANUM_GAME_SCHEMATIC_H_ */
