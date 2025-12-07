#ifndef TAB_TILES_GAME_H
#define TAB_TILES_GAME_H

#include <lvgl.h>

// Baut den neuen Game-Tile-Tab auf (unified tile system)
void build_tiles_game_tab(lv_obj_t* parent);

// Lädt das Game-Tile-Layout neu
void tiles_game_reload_layout();

#endif // TAB_TILES_GAME_H
