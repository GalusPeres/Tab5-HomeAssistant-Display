#include "src/ui/tab_tiles_home.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/tile_renderer.h"
#include <Arduino.h>

/* === Layout-Konstanten === */
static const int GAP = 24;
static const int OUTER = 0;
static const int GRID_PAD = 24;

/* === Globale State === */
static lv_obj_t* g_tiles_home_grid = nullptr;
static scene_publish_cb_t g_tiles_scene_cb = nullptr;

/* === Aufbau Tiles-Home-Tab === */
void build_tiles_home_tab(lv_obj_t *parent, scene_publish_cb_t scene_cb) {
  g_tiles_scene_cb = scene_cb;

  lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  lv_obj_set_scroll_dir(parent, LV_DIR_VER);
  lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_anim_duration(parent, 0, 0);
  lv_obj_set_style_pad_all(parent, OUTER, 0);

  g_tiles_home_grid = lv_obj_create(parent);
  lv_obj_set_style_bg_color(g_tiles_home_grid, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(g_tiles_home_grid, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_tiles_home_grid, 0, 0);
  lv_obj_set_style_pad_all(g_tiles_home_grid, GRID_PAD, 0);
  lv_obj_remove_flag(g_tiles_home_grid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(g_tiles_home_grid, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_column(g_tiles_home_grid, GAP, 0);
  lv_obj_set_style_pad_row(g_tiles_home_grid, GAP, 0);

  static lv_coord_t col_dsc[] = {
    LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST
  };
  static lv_coord_t row_dsc[] = {
    LV_GRID_CONTENT,
    LV_GRID_CONTENT,
    LV_GRID_CONTENT,
    LV_GRID_CONTENT,
    LV_GRID_TEMPLATE_LAST
  };
  lv_obj_set_grid_dsc_array(g_tiles_home_grid, col_dsc, row_dsc);

  tiles_home_reload_layout();
}

void tiles_home_reload_layout() {
  if (!g_tiles_home_grid) return;

  lv_obj_clean(g_tiles_home_grid);

  // Render tile grid using new unified system
  const TileGridConfig& config = tileConfig.getHomeGrid();
  render_tile_grid(g_tiles_home_grid, config, GridType::HOME, g_tiles_scene_cb);

  Serial.println("[TilesHome] Layout neu geladen");
}

void tiles_home_update_sensor_by_entity(const char* entity_id, const char* value) {
  if (!entity_id || !value) return;

  const TileGridConfig& config = tileConfig.getHomeGrid();

  // Find tile with matching sensor_entity
  for (uint8_t i = 0; i < TILES_PER_GRID; i++) {
    const Tile& tile = config.tiles[i];
    if (tile.type == TILE_SENSOR && tile.sensor_entity.equalsIgnoreCase(entity_id)) {
      // Übergebe Wert + Einheit aus Config (wie im Webinterface)
      // WICHTIG: Queue statt direktem Update (verhindert LVGL Race Condition!)
      const char* unit = tile.sensor_unit.length() > 0 ? tile.sensor_unit.c_str() : nullptr;
      queue_sensor_tile_update(GridType::HOME, i, value, unit);
      Serial.printf("[TilesHome] Sensor %s@%u queued: %s %s\n", entity_id, i, value, unit ? unit : "");
    }
  }
}
