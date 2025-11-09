#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <lvgl.h>
#include <Arduino.h>

// Forward Declarations
typedef void (*scene_publish_cb_t)(const char* scene_name);
typedef void (*hotspot_start_cb_t)();

// UI Manager - Verwaltet die Benutzeroberfläche
class UIManager {
public:
  // UI aufbauen
  void buildUI(scene_publish_cb_t scene_cb, hotspot_start_cb_t hotspot_cb = nullptr);

  // Statusbar
  void updateStatusbar();

  // NTP Sync
  void scheduleNtpSync(uint32_t delay_ms = 0);
  void serviceNtpSync();

private:
  // Statusbar-Elemente
  lv_obj_t *status_container = nullptr;
  lv_obj_t *status_time_label = nullptr;
  lv_obj_t *status_date_label = nullptr;

  // NTP-Sync
  uint32_t next_ntp_sync_ms = 0;
  bool tz_configured = false;
  static const char* TZ_EUROPE_BERLIN;

  // Interne Funktionen
  void statusbarInit(lv_obj_t *tab_bar);
};

// Globale Instanz
extern UIManager uiManager;

#endif // UI_MANAGER_H
