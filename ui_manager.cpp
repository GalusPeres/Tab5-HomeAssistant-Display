#include "ui_manager.h"
#include "tab_home.h"
#include "tab_solar.h"
#include "tab_weather.h"
#include "tab_settings.h"
#include <WiFi.h>
#include <time.h>
#include <M5Unified.h>

// Globale Instanz
UIManager uiManager;

// Timezone
const char* UIManager::TZ_EUROPE_BERLIN = "CET-1CEST,M3.5.0/02,M10.5.0/03";

// ========== UI aufbauen ==========
void UIManager::buildUI(scene_publish_cb_t scene_cb, hotspot_start_cb_t hotspot_cb) {
  Serial.println("🎨 Baue UI auf...");

  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x111111), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  // TabView links, breiter Balken
  lv_obj_t *tabview = lv_tabview_create(scr);
  lv_tabview_set_tab_bar_position(tabview, LV_DIR_LEFT);
  lv_tabview_set_tab_bar_size(tabview, 180);
  lv_obj_set_size(tabview, 1280, 720);  // SCREEN_WIDTH, SCREEN_HEIGHT

  // Tab-Bar + Content deckend (keine Transparenzen)
  lv_obj_t *tab_buttons = lv_tabview_get_tab_bar(tabview);
  lv_obj_set_style_bg_color(tab_buttons, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_bg_opa(tab_buttons, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(tab_buttons, lv_color_white(), 0);

  // Große Tab-Symbole (48 pt mit Fallback auf 32 pt)
#if defined(LV_FONT_MONTSERRAT_48) && LV_FONT_MONTSERRAT_48
  lv_obj_set_style_text_font(tab_buttons, &lv_font_montserrat_48, 0);
#elif defined(LV_FONT_MONTSERRAT_32) && LV_FONT_MONTSERRAT_32
  lv_obj_set_style_text_font(tab_buttons, &lv_font_montserrat_32, 0);
#endif

  statusbarInit(tab_buttons);

  lv_obj_t *tab_content = lv_tabview_get_content(tabview);
  lv_obj_set_style_bg_color(tab_content, lv_color_hex(0x111111), 0);
  lv_obj_set_style_bg_opa(tab_content, LV_OPA_COVER, 0);
  lv_obj_clear_flag(tab_content, LV_OBJ_FLAG_SCROLLABLE);

  // Tabs nur mit Symbolen
  lv_obj_t *tab1 = lv_tabview_add_tab(tabview, LV_SYMBOL_HOME);      // Home
  lv_obj_t *tab2 = lv_tabview_add_tab(tabview, LV_SYMBOL_CHARGE);    // Solar/Energie
  lv_obj_t *tab3 = lv_tabview_add_tab(tabview, LV_SYMBOL_DOWNLOAD);  // Wetter
  lv_obj_t *tab4 = lv_tabview_add_tab(tabview, LV_SYMBOL_SETTINGS);  // Einstellungen

  // Inhalte laden
  build_home_tab(tab1, scene_cb);
  build_solar_tab(tab2);
  build_weather_tab(tab3);
  build_settings_tab(tab4, hotspot_cb);

  Serial.println("✓ UI aufgebaut");
}

// ========== Statusbar initialisieren ==========
void UIManager::statusbarInit(lv_obj_t *tab_bar) {
  if (status_container || !tab_bar) return;

  status_container = lv_obj_create(tab_bar);
  lv_obj_set_size(status_container, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(status_container, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_bg_opa(status_container, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(status_container, 0, 0);
  lv_obj_set_style_radius(status_container, 12, 0);
  lv_obj_set_style_pad_all(status_container, 12, 0);
  lv_obj_set_style_pad_row(status_container, 4, 0);
  lv_obj_set_flex_flow(status_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(status_container, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(status_container, LV_OBJ_FLAG_CLICKABLE);

  status_time_label = lv_label_create(status_container);
  lv_obj_set_width(status_time_label, LV_PCT(100));
  lv_label_set_long_mode(status_time_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(status_time_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(status_time_label, lv_color_white(), 0);
#if defined(LV_FONT_MONTSERRAT_48) && LV_FONT_MONTSERRAT_48
  lv_obj_set_style_text_font(status_time_label, &lv_font_montserrat_48, 0);
#elif defined(LV_FONT_MONTSERRAT_32) && LV_FONT_MONTSERRAT_32
  lv_obj_set_style_text_font(status_time_label, &lv_font_montserrat_32, 0);
#endif
  lv_label_set_text(status_time_label, "--:--");

  status_date_label = lv_label_create(status_container);
  lv_obj_set_width(status_date_label, LV_PCT(100));
  lv_label_set_long_mode(status_date_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(status_date_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(status_date_label, lv_color_hex(0xC8C8C8), 0);
#if defined(LV_FONT_MONTSERRAT_24) && LV_FONT_MONTSERRAT_24
  lv_obj_set_style_text_font(status_date_label, &lv_font_montserrat_24, 0);
#elif defined(LV_FONT_MONTSERRAT_20) && LV_FONT_MONTSERRAT_20
  lv_obj_set_style_text_font(status_date_label, &lv_font_montserrat_20, 0);
#elif defined(LV_FONT_MONTSERRAT_14) && LV_FONT_MONTSERRAT_14
  lv_obj_set_style_text_font(status_date_label, &lv_font_montserrat_14, 0);
#endif
  lv_label_set_text(status_date_label, "--.--.----");
}

// ========== Statusbar aktualisieren ==========
void UIManager::updateStatusbar() {
  if (!status_time_label || !status_date_label) return;

  char buf[48];
  bool have_time = false;
  int hour = 0, minute = 0, day = 0, month = 0, year = 0;

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 0)) {
    have_time = true;
    hour = timeinfo.tm_hour;
    minute = timeinfo.tm_min;
    day = timeinfo.tm_mday;
    month = timeinfo.tm_mon + 1;
    year = timeinfo.tm_year + 1900;
  } else if (M5.Rtc.isEnabled()) {
    m5::rtc_datetime_t dt;
    if (M5.Rtc.getDateTime(&dt)) {
      have_time = true;
      hour = dt.time.hours;
      minute = dt.time.minutes;
      day = dt.date.date;
      month = dt.date.month;
      year = dt.date.year;
    }
  }

  if (have_time) {
    snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);
  } else {
    snprintf(buf, sizeof(buf), "--:--");
  }
  lv_label_set_text(status_time_label, buf);

  if (have_time) {
    snprintf(buf, sizeof(buf), "%02d.%02d.%04d", day, month, year);
  } else {
    snprintf(buf, sizeof(buf), "--.--.----");
  }
  lv_label_set_text(status_date_label, buf);

  // NTP-Sync triggern wenn keine Zeit aber WiFi verbunden
  if (!have_time && WiFi.status() == WL_CONNECTED) {
    scheduleNtpSync(0);
  }
}

// ========== NTP-Sync ==========
void UIManager::scheduleNtpSync(uint32_t delay_ms) {
  next_ntp_sync_ms = millis() + delay_ms;
}

void UIManager::serviceNtpSync() {
  if (WiFi.status() != WL_CONNECTED) return;

  uint32_t now_ms = millis();
  if ((int32_t)(now_ms - next_ntp_sync_ms) < 0) return;

  configTzTime(TZ_EUROPE_BERLIN, "pool.ntp.org", "time.nist.gov", "time.cloudflare.com");
  tz_configured = true;
  next_ntp_sync_ms = now_ms + 3600000UL; // Stündlich neu syncen
}
