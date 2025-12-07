#include "src/ui/tab_tiles_game.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/tile_renderer.h"
#include <Arduino.h>

/* === Layout-Konstanten === */
static const int GAP = 24;
static const int OUTER = 0;
static const int GRID_PAD = 24;

/* === Globale State === */
static lv_obj_t* g_tiles_game_grid = nullptr;

/* === Aufbau Tiles-Game-Tab === */
void build_tiles_game_tab(lv_obj_t *parent) {
  lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  lv_obj_set_scroll_dir(parent, LV_DIR_VER);
  lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_anim_duration(parent, 0, 0);
  lv_obj_set_style_pad_all(parent, OUTER, 0);

  g_tiles_game_grid = lv_obj_create(parent);
  lv_obj_set_style_bg_color(g_tiles_game_grid, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(g_tiles_game_grid, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_tiles_game_grid, 0, 0);
  lv_obj_set_style_pad_all(g_tiles_game_grid, GRID_PAD, 0);
  lv_obj_remove_flag(g_tiles_game_grid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(g_tiles_game_grid, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_column(g_tiles_game_grid, GAP, 0);
  lv_obj_set_style_pad_row(g_tiles_game_grid, GAP, 0);

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
  lv_obj_set_grid_dsc_array(g_tiles_game_grid, col_dsc, row_dsc);

  tiles_game_reload_layout();
}

void tiles_game_reload_layout() {
  if (!g_tiles_game_grid) return;

  lv_obj_clean(g_tiles_game_grid);

  // Render tile grid using new unified system
  const TileGridConfig& config = tileConfig.getGameGrid();
  render_tile_grid(g_tiles_game_grid, config, GridType::GAME, nullptr);

  Serial.println("[TilesGame] Layout neu geladen");
}
