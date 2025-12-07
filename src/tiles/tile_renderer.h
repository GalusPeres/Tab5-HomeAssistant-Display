#ifndef TILE_RENDERER_H
#define TILE_RENDERER_H

#include <lvgl.h>
#include "src/tiles/tile_config.h"

// Forward declarations
typedef void (*scene_publish_cb_t)(const char* scene_alias);

enum class GridType : uint8_t {
  HOME = 0,
  GAME = 1
};

// Rendert ein komplettes Tile-Grid (12 Kacheln, 3×4)
void render_tile_grid(lv_obj_t* parent, const TileGridConfig& config, GridType grid_type, scene_publish_cb_t scene_cb = nullptr);

// Rendert eine einzelne Kachel basierend auf Typ
void render_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type, scene_publish_cb_t scene_cb);

// Typ-spezifische Render-Funktionen
void render_sensor_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type);
void render_scene_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, scene_publish_cb_t scene_cb);
void render_key_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type);
void render_empty_tile(lv_obj_t* parent, int col, int row);

// Update-Funktionen (für Sensoren)
void update_sensor_tile_value(uint8_t grid_index, const char* value, const char* unit = nullptr);

#endif // TILE_RENDERER_H
