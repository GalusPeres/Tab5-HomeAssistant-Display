#include "src/tiles/tile_renderer.h"
#include "src/network/ha_bridge_config.h"
#include "src/game/game_ws_server.h"
#include <Arduino.h>

/* === Layout-Konstanten === */
static const int CARD_H = 150;

/* === Fonts === */
#if defined(LV_FONT_MONTSERRAT_24) && LV_FONT_MONTSERRAT_24
  #define FONT_TITLE (&lv_font_montserrat_24)
#else
  #define FONT_TITLE (LV_FONT_DEFAULT)
#endif

#if defined(LV_FONT_MONTSERRAT_48) && LV_FONT_MONTSERRAT_48
  #define FONT_VALUE (&lv_font_montserrat_48)
#elif defined(LV_FONT_MONTSERRAT_40) && LV_FONT_MONTSERRAT_40
  #define FONT_VALUE (&lv_font_montserrat_40)
#else
  #define FONT_VALUE (LV_FONT_DEFAULT)
#endif

#if defined(LV_FONT_MONTSERRAT_28) && LV_FONT_MONTSERRAT_28
  #define FONT_UNIT (&lv_font_montserrat_28)
#elif defined(LV_FONT_MONTSERRAT_24) && LV_FONT_MONTSERRAT_24
  #define FONT_UNIT (&lv_font_montserrat_24)
#else
  #define FONT_UNIT (FONT_TITLE)
#endif

/* === Globale State für Updates === */
struct SensorTileWidgets {
  lv_obj_t* value_label = nullptr;
  lv_obj_t* unit_label = nullptr;
};

static SensorTileWidgets g_home_sensors[TILES_PER_GRID];
static SensorTileWidgets g_game_sensors[TILES_PER_GRID];

/* === Helfer === */
static void set_label_style(lv_obj_t* lbl, lv_color_t c, const lv_font_t* f) {
  lv_obj_set_style_text_color(lbl, c, 0);
  lv_obj_set_style_text_font(lbl, f, 0);
}

void render_tile_grid(lv_obj_t* parent, const TileGridConfig& config, GridType grid_type, scene_publish_cb_t scene_cb) {
  for (int i = 0; i < TILES_PER_GRID; ++i) {
    int row = i / 3;
    int col = i % 3;
    render_tile(parent, col, row, config.tiles[i], i, grid_type, scene_cb);
  }
}

void render_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type, scene_publish_cb_t scene_cb) {
  switch (tile.type) {
    case TILE_SENSOR:
      render_sensor_tile(parent, col, row, tile, index, grid_type);
      break;
    case TILE_SCENE:
      render_scene_tile(parent, col, row, tile, index, scene_cb);
      break;
    case TILE_KEY:
      render_key_tile(parent, col, row, tile, index, grid_type);
      break;
    default:
      render_empty_tile(parent, col, row);
  }
}

void render_sensor_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type) {
  lv_obj_t* card = lv_obj_create(parent);

  // Farbe verwenden (Standard: 0x2A2A2A wenn color = 0)
  uint32_t card_color = (tile.bg_color != 0) ? tile.bg_color : 0x2A2A2A;
  lv_obj_set_style_bg_color(card, lv_color_hex(card_color), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 22, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_set_style_pad_hor(card, 20, 0);
  lv_obj_set_style_pad_ver(card, 24, 0);
  lv_obj_set_height(card, CARD_H);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_grid_cell(card,
      LV_GRID_ALIGN_STRETCH, col, 1,
      LV_GRID_ALIGN_STRETCH, row, 1);

  // Title
  lv_obj_t* t = lv_label_create(card);
  set_label_style(t, lv_color_hex(0xFFFFFF), FONT_TITLE);
  lv_label_set_text(t, tile.title.length() ? tile.title.c_str() : "Sensor");
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

  // Container für Wert + Einheit (Flex Layout für sauberes Nebeneinander)
  lv_obj_t* container = lv_obj_create(card);
  lv_obj_set_size(container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(container, 0, 0);
  lv_obj_set_style_pad_all(container, 0, 0);
  lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(container, 6, 0);  // 6px Abstand zwischen Wert und Einheit
  lv_obj_align(container, LV_ALIGN_CENTER, 0, 18);

  // Wert-Label (große Schrift)
  lv_obj_t* v = lv_label_create(container);
  set_label_style(v, lv_color_white(), FONT_VALUE);
  lv_label_set_text(v, "--");

  // Einheit-Label (kleinere Schrift)
  lv_obj_t* u = lv_label_create(container);
  set_label_style(u, lv_color_hex(0xE6E6E6), FONT_UNIT);
  lv_label_set_text(u, "");

  // Speichern für spätere Updates
  SensorTileWidgets* target = (grid_type == GridType::HOME) ? g_home_sensors : g_game_sensors;
  target[index].value_label = v;
  target[index].unit_label = u;
}

struct SceneEventData {
  String scene_alias;
  scene_publish_cb_t callback;
};

void render_scene_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, scene_publish_cb_t scene_cb) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_style_radius(btn, 22, 0);
  lv_obj_set_style_border_width(btn, 0, 0);

  // Farbe verwenden (Standard: 0x353535 wenn color = 0)
  uint32_t btn_color = (tile.bg_color != 0) ? tile.bg_color : 0x353535;
  lv_obj_set_style_bg_color(btn, lv_color_hex(btn_color), LV_PART_MAIN | LV_STATE_DEFAULT);

  // Pressed-State: 10% heller
  uint32_t pressed_color = btn_color + 0x101010;
  lv_obj_set_style_bg_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_height(btn, CARD_H);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_grid_cell(btn,
      LV_GRID_ALIGN_STRETCH, col, 1,
      LV_GRID_ALIGN_STRETCH, row, 1);

  lv_obj_t* l = lv_label_create(btn);
  set_label_style(l, lv_color_white(), FONT_TITLE);
  lv_label_set_text(l, tile.title.length() ? tile.title.c_str() : "Szene");
  lv_obj_center(l);

  // Event-Handler für Scene-Aktivierung
  if (scene_cb && tile.scene_alias.length()) {
    // Allocate permanent storage for event data
    SceneEventData* event_data = new SceneEventData{tile.scene_alias, scene_cb};

    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
          SceneEventData* data = static_cast<SceneEventData*>(lv_event_get_user_data(e));
          if (data && data->callback) {
            Serial.printf("[Tile] Szene aktiviert: %s\n", data->scene_alias.c_str());
            data->callback(data->scene_alias.c_str());
          }
        },
        LV_EVENT_CLICKED,
        event_data);
  }
}

struct KeyEventData {
  String title;
  uint8_t key_code;
  uint8_t modifier;
  uint8_t index;
};

void render_key_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_style_radius(btn, 22, 0);
  lv_obj_set_style_border_width(btn, 0, 0);

  // Farbe verwenden (Standard: 0x353535 wenn color = 0)
  uint32_t btn_color = (tile.bg_color != 0) ? tile.bg_color : 0x353535;
  lv_obj_set_style_bg_color(btn, lv_color_hex(btn_color), LV_PART_MAIN | LV_STATE_DEFAULT);

  // Pressed-State: 10% heller
  uint32_t pressed_color = btn_color + 0x101010;
  lv_obj_set_style_bg_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_height(btn, CARD_H);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_grid_cell(btn,
      LV_GRID_ALIGN_STRETCH, col, 1,
      LV_GRID_ALIGN_STRETCH, row, 1);

  lv_obj_t* l = lv_label_create(btn);
  set_label_style(l, lv_color_white(), FONT_TITLE);
  lv_label_set_text(l, tile.title.length() ? tile.title.c_str() : "Key");
  lv_obj_center(l);

  // Event-Handler für WebSocket Broadcast
  if (tile.key_code != 0) {
    // Allocate permanent storage for event data
    KeyEventData* event_data = new KeyEventData{
      tile.title,
      tile.key_code,
      tile.key_modifier,
      index
    };

    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
          KeyEventData* data = static_cast<KeyEventData*>(lv_event_get_user_data(e));
          if (data) {
            Serial.printf("[Tile] Key '%s' gedrückt - Code: 0x%02X Mod: 0x%02X\n",
                          data->title.c_str(), data->key_code, data->modifier);

            // WebSocket Broadcast an alle verbundenen Clients
            gameWSServer.broadcastButtonPress(
              data->index,
              data->title.c_str(),
              data->key_code,
              data->modifier
            );
          }
        },
        LV_EVENT_CLICKED,
        event_data);
  }
}

void render_empty_tile(lv_obj_t* parent, int col, int row) {
  lv_obj_t* placeholder = lv_obj_create(parent);
  lv_obj_set_style_bg_opa(placeholder, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(placeholder, 0, 0);
  lv_obj_set_height(placeholder, CARD_H);
  lv_obj_set_grid_cell(placeholder,
      LV_GRID_ALIGN_STRETCH, col, 1,
      LV_GRID_ALIGN_STRETCH, row, 1);
}

void update_sensor_tile_value(uint8_t grid_index, const char* value, const char* unit) {
  // TODO: Bestimme ob Home oder Game Grid
  if (grid_index < TILES_PER_GRID && g_home_sensors[grid_index].value_label) {
    String displayValue = String(value);
    displayValue.trim();

    // Zeige "--" wenn leer oder unavailable
    if (displayValue.length() == 0 || displayValue.equalsIgnoreCase("unavailable")) {
      displayValue = "--";
    }

    // Setze Wert (große Schrift)
    lv_label_set_text(g_home_sensors[grid_index].value_label, displayValue.c_str());

    // Setze Einheit (kleine Schrift, separates Label)
    if (g_home_sensors[grid_index].unit_label) {
      if (unit && strlen(unit) > 0 && displayValue != "--") {
        lv_label_set_text(g_home_sensors[grid_index].unit_label, unit);
      } else {
        lv_label_set_text(g_home_sensors[grid_index].unit_label, "");
      }
    }
  }
}
