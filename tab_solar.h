#pragma once
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

void build_solar_tab(lv_obj_t *parent);
void solar_update_power(float watts);
void solar_set_history_csv(const char* csv);

#ifdef __cplusplus
}
#endif
