#ifndef TAB_TILES_HOME_H
#define TAB_TILES_HOME_H

#include <lvgl.h>

// Callback-Typ für Szenen-Publishing
typedef void (*scene_publish_cb_t)(const char* scene_alias);

// Baut den neuen Home-Tile-Tab auf (unified tile system)
void build_tiles_home_tab(lv_obj_t* parent, scene_publish_cb_t scene_cb);

// Lädt das Home-Tile-Layout neu
void tiles_home_reload_layout();
void tiles_home_update_tile(uint8_t index);

// Update Sensor-Wert per Entity-ID (für MQTT-Handler)
void tiles_home_update_sensor_by_entity(const char* entity_id, const char* value);

#endif // TAB_TILES_HOME_H
