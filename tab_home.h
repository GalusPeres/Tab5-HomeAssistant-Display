#pragma once
#include <lvgl.h>
typedef void (*scene_publish_cb_t)(const char* scene_name);

void build_home_tab(lv_obj_t *parent, scene_publish_cb_t scene_cb);

// Update values on the Home tab (called from MQTT or other data sources)
void home_set_values(float outside_c, float inside_c, int soc_pct);

// Update dedicated HA sensor (Wohnbereich Temperatur)
void home_set_wohn_temp(float celsius);

// Update PV Haus/Garage power (W)
void home_set_pv_garage(float watts);
