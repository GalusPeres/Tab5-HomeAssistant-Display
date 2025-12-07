#ifndef TAB_TILES_GAME_H
#define TAB_TILES_GAME_H

#include <lvgl.h>

// Callback-Typ für Szenen-Publishing
typedef void (*scene_publish_cb_t)(const char* scene_alias);

// Baut den neuen Game-Tile-Tab auf (unified tile system)
void build_tiles_game_tab(lv_obj_t* parent, scene_publish_cb_t scene_cb);

// Lädt das Game-Tile-Layout neu
void tiles_game_reload_layout();

// Update Sensor-Wert per Entity-ID (für MQTT-Handler)
void tiles_game_update_sensor_by_entity(const char* entity_id, const char* value);

#endif // TAB_TILES_GAME_H
