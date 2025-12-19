#include <M5Unified.h>
#include <lvgl.h>
#include "src/ui/tab_settings.h"
#include "src/core/config_manager.h"

// Containers
static lv_obj_t *main_menu = nullptr;
static lv_obj_t *network_page = nullptr;
static lv_obj_t *display_page = nullptr;
static lv_obj_t *energy_page = nullptr;

static lv_obj_t *brightness_label = nullptr;
static hotspot_callback_t g_hotspot_callback = nullptr;

// WiFi Status Labels
static lv_obj_t *wifi_status_label = nullptr;
static lv_obj_t *wifi_ssid_label = nullptr;
static lv_obj_t *wifi_ip_label = nullptr;
static lv_obj_t *ap_mode_btn = nullptr;
static lv_obj_t *ap_mode_btn_label = nullptr;
static lv_obj_t *ap_confirm_row = nullptr;
static lv_obj_t *ap_confirm_yes_btn = nullptr;
static lv_obj_t *ap_confirm_no_btn = nullptr;
static lv_obj_t *confirm_panel = nullptr;
static lv_obj_t *confirm_yes_btn = nullptr;
static lv_obj_t *confirm_no_btn = nullptr;
static lv_obj_t *mqtt_notice_label = nullptr;
static bool ap_mode_confirm_pending = false;
static bool ap_mode_active = false;

// Sleep Settings
static lv_obj_t *sleep_checkbox = nullptr;
static lv_obj_t *sleep_slider = nullptr;
static lv_obj_t *sleep_time_label = nullptr;
static lv_obj_t *sleep_label = nullptr;
static lv_obj_t *sleep_battery_checkbox = nullptr;
static lv_obj_t *sleep_battery_slider = nullptr;
static lv_obj_t *sleep_battery_time_label = nullptr;
static lv_obj_t *sleep_battery_label = nullptr;

// Power Status Labels
static lv_obj_t *power_status_label = nullptr;
static lv_obj_t *power_voltage_label = nullptr;
static lv_obj_t *power_level_label = nullptr;
static lv_obj_t *power_status_box = nullptr;

static const int kSettingsPadX = 20;
static const int kSettingsTitleY = 8;
static const int kSettingsRowY = 44;
static const int kSettingsSliderY = 78;
static const int kSettingsStatusY = 114;
static const int kSettingsStatusGap = 18;

// Forward declarations
static void show_network_page();
static void show_display_page();
static void show_energy_page();
static void show_main_menu();
void settings_update_ap_mode(bool running);

static uint16_t sleep_seconds_from_index(int32_t index) {
  if (index < 0) {
    index = 0;
  } else if (index >= static_cast<int32_t>(kSleepOptionsSecCount)) {
    index = static_cast<int32_t>(kSleepOptionsSecCount) - 1;
  }
  return kSleepOptionsSec[index];
}

static int32_t sleep_index_from_seconds(uint16_t seconds) {
  uint16_t closest = kSleepOptionsSec[0];
  int32_t closest_index = 0;
  uint16_t best_diff = (seconds > closest) ? (seconds - closest) : (closest - seconds);
  for (size_t i = 1; i < kSleepOptionsSecCount; ++i) {
    uint16_t option = kSleepOptionsSec[i];
    uint16_t diff = (seconds > option) ? (seconds - option) : (option - seconds);
    if (diff < best_diff) {
      best_diff = diff;
      closest = option;
      closest_index = static_cast<int32_t>(i);
    }
  }
  return closest_index;
}

static void format_sleep_label(char* buf, size_t len, uint16_t seconds) {
  if (seconds <= 60) {
    snprintf(buf, len, "%u s", static_cast<unsigned>(seconds));
  } else {
    snprintf(buf, len, "%u min", static_cast<unsigned>(seconds / 60));
  }
}

static int32_t sleep_slider_max_index() {
  return static_cast<int32_t>(kSleepOptionsSecCount);
}

static bool sleep_index_is_never(int32_t index) {
  return index >= static_cast<int32_t>(kSleepOptionsSecCount);
}

static void format_sleep_label_for_index(char* buf, size_t len, int32_t index) {
  if (sleep_index_is_never(index)) {
    snprintf(buf, len, "Nie");
    return;
  }
  format_sleep_label(buf, len, sleep_seconds_from_index(index));
}

static void on_brightness(lv_event_t *e) {
  lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
  lv_event_code_t code = lv_event_get_code(e);
  int32_t value = lv_slider_get_value(slider);

  // Helligkeit sofort ändern
  M5.Display.setBrightness(value);

  // Label sofort updaten
  static char buf[16];
  snprintf(buf, sizeof(buf), "%d", (int)value);
  if (brightness_label) lv_label_set_text(brightness_label, buf);

  // Config NUR beim Loslassen speichern (nicht bei jeder Bewegung!)
  if (code == LV_EVENT_RELEASED) {
    const DeviceConfig& cfg = configManager.getConfig();
    configManager.saveDisplaySettings(
        value,
        cfg.auto_sleep_enabled,
        cfg.auto_sleep_seconds,
        cfg.auto_sleep_battery_enabled,
        cfg.auto_sleep_battery_seconds);
  }
}

static void toggle_sleep_controls(lv_obj_t* slider, lv_obj_t* time_label, lv_obj_t* label, bool enabled) {
  if (!slider || !time_label || !label) return;
  if (enabled) {
    lv_obj_clear_flag(slider, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(slider, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
  }
}

static void on_sleep_checkbox(lv_event_t *e) {
  lv_obj_t *cb = (lv_obj_t*)lv_event_get_target(e);
  bool enabled = lv_obj_get_state(cb) & LV_STATE_CHECKED;

  toggle_sleep_controls(sleep_slider, sleep_time_label, sleep_label, enabled);

  // Speichere in Config
  const DeviceConfig& cfg = configManager.getConfig();
  configManager.saveDisplaySettings(
      cfg.display_brightness,
      enabled,
      cfg.auto_sleep_seconds,
      cfg.auto_sleep_battery_enabled,
      cfg.auto_sleep_battery_seconds);
}

static void on_sleep_slider(lv_event_t *e) {
  lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
  lv_event_code_t code = lv_event_get_code(e);
  int32_t index = lv_slider_get_value(slider);

  // Label sofort updaten
  static char buf[16];
  format_sleep_label_for_index(buf, sizeof(buf), index);
  if (sleep_time_label) lv_label_set_text(sleep_time_label, buf);

  // Config NUR beim Loslassen speichern (nicht bei jeder Bewegung!)
  if (code == LV_EVENT_RELEASED) {
    const DeviceConfig& cfg = configManager.getConfig();
    bool enabled = !sleep_index_is_never(index);
    uint16_t seconds = enabled ? sleep_seconds_from_index(index) : cfg.auto_sleep_seconds;
    configManager.saveDisplaySettings(
        cfg.display_brightness,
        enabled,
        seconds,
        cfg.auto_sleep_battery_enabled,
        cfg.auto_sleep_battery_seconds);
  }
}

static void on_sleep_battery_checkbox(lv_event_t *e) {
  lv_obj_t *cb = (lv_obj_t*)lv_event_get_target(e);
  bool enabled = lv_obj_get_state(cb) & LV_STATE_CHECKED;

  toggle_sleep_controls(sleep_battery_slider, sleep_battery_time_label, sleep_battery_label, enabled);

  const DeviceConfig& cfg = configManager.getConfig();
  configManager.saveDisplaySettings(
      cfg.display_brightness,
      cfg.auto_sleep_enabled,
      cfg.auto_sleep_seconds,
      enabled,
      cfg.auto_sleep_battery_seconds);
}

static void on_sleep_battery_slider(lv_event_t *e) {
  lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
  lv_event_code_t code = lv_event_get_code(e);
  int32_t index = lv_slider_get_value(slider);

  static char buf[16];
  format_sleep_label_for_index(buf, sizeof(buf), index);
  if (sleep_battery_time_label) lv_label_set_text(sleep_battery_time_label, buf);

  if (code == LV_EVENT_RELEASED) {
    const DeviceConfig& cfg = configManager.getConfig();
    bool enabled = !sleep_index_is_never(index);
    uint16_t seconds = enabled ? sleep_seconds_from_index(index) : cfg.auto_sleep_battery_seconds;
    configManager.saveDisplaySettings(
        cfg.display_brightness,
        cfg.auto_sleep_enabled,
        cfg.auto_sleep_seconds,
        enabled,
        seconds);
  }
}

// Power Status Update (public, wird von main loop aufgerufen)
void settings_update_power_status() {
  if (!power_status_label || !power_level_label) return;

  int32_t batLevel = M5.Power.getBatteryLevel();
  int32_t batCurrent = M5.Power.getBatteryCurrent();

  static char status_buf[40];
  static char level_buf[32];

  static bool lastPoweredState = false;
  static uint32_t lastStateChange = 0;
  uint32_t now = millis();

  bool currentPowered = (batCurrent <= 50);
  if (currentPowered != lastPoweredState) {
    if (now - lastStateChange > 2000) {
      lastPoweredState = currentPowered;
      lastStateChange = now;
    }
  } else {
    lastStateChange = now;
  }

  int displayLevel = (batLevel >= 0 && batLevel <= 100) ? batLevel : 0;
  const char* statusText = lastPoweredState ? "Netzteil" : "Akku";

  snprintf(status_buf, sizeof(status_buf), "Status: %s", statusText);
  snprintf(level_buf, sizeof(level_buf), "Level: %d%%", displayLevel);

  lv_label_set_text(power_status_label, status_buf);
  lv_label_set_text(power_level_label, level_buf);
}

static void hide_confirm_panel() {
  if (ap_confirm_row) {
    lv_obj_add_flag(ap_confirm_row, LV_OBJ_FLAG_HIDDEN);
  }
  if (ap_mode_btn && !ap_mode_active) {
    lv_obj_clear_flag(ap_mode_btn, LV_OBJ_FLAG_HIDDEN);
  }
  if (confirm_panel) {
    lv_obj_add_flag(confirm_panel, LV_OBJ_FLAG_HIDDEN);
  }
  ap_mode_confirm_pending = false;
}

static void on_confirm_yes_clicked(lv_event_t *e) {
  if (g_hotspot_callback) {
    g_hotspot_callback(true);
  }
  settings_update_ap_mode(true);
  hide_confirm_panel();
}

static void on_confirm_no_clicked(lv_event_t *e) {
  hide_confirm_panel();
}

static void on_ap_mode_clicked(lv_event_t *e) {
  if (ap_mode_active) {
    if (g_hotspot_callback) {
      g_hotspot_callback(false);
    }
    return;
  }
  if (ap_confirm_row && !ap_mode_confirm_pending) {
    ap_mode_confirm_pending = true;
    lv_obj_clear_flag(ap_confirm_row, LV_OBJ_FLAG_HIDDEN);
    if (ap_mode_btn) {
      lv_obj_add_flag(ap_mode_btn, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static lv_obj_t *create_settings_card(lv_obj_t *parent, uint8_t col, uint8_t row) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_grid_cell(card, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_border_opa(card, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(card, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(card, 22, 0);
  lv_obj_set_style_pad_hor(card, 20, 0);
  lv_obj_set_style_pad_ver(card, 18, 0);
  lv_obj_set_style_pad_row(card, 8, 0);
  lv_obj_set_style_pad_column(card, 10, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  return card;
}

static lv_obj_t *create_card_row(lv_obj_t *parent) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return row;
}

static lv_obj_t *create_slider_row(lv_obj_t *parent) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_bottom(row, 2, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return row;
}

static lv_obj_t *create_status_box(lv_obj_t *parent) {
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(box, LV_PCT(100));
  lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(box, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(box, 0, 0);
  lv_obj_set_style_pad_row(box, 2, 0);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  return box;
}

static void style_settings_slider(lv_obj_t *slider) {
  lv_obj_set_width(slider, LV_PCT(80));
  lv_obj_set_height(slider, 18);
  lv_obj_set_style_width(slider, 26, LV_PART_KNOB);
  lv_obj_set_style_height(slider, 26, LV_PART_KNOB);
}

// ========== Button Event Handlers ==========
static void on_network_btn_clicked(lv_event_t *e) {
  hide_confirm_panel();
  show_network_page();
}

static void on_display_btn_clicked(lv_event_t *e) {
  hide_confirm_panel();
  show_display_page();
}

static void on_energy_btn_clicked(lv_event_t *e) {
  hide_confirm_panel();
  show_energy_page();
}

static void on_back_btn_clicked(lv_event_t *e) {
  hide_confirm_panel();
  show_main_menu();
}

// ========== Main Menu (Grid mit 3 Buttons) ==========
static void create_main_menu(lv_obj_t *parent) {
  main_menu = lv_obj_create(parent);
  lv_obj_set_size(main_menu, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(main_menu, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(main_menu, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(main_menu, LV_OPA_COVER, 0);
  lv_obj_set_style_border_opa(main_menu, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(main_menu, 20, 0);

  // Grid Setup (2x4)
  static int32_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static int32_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(main_menu, col_dsc, row_dsc);
  lv_obj_set_style_pad_column(main_menu, 20, 0);
  lv_obj_set_style_pad_row(main_menu, 20, 0);

  // Button 1: Netzwerk (Position 0,0)
  lv_obj_t *btn_network = lv_button_create(main_menu);
  lv_obj_set_grid_cell(btn_network, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
  lv_obj_set_style_bg_color(btn_network, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_border_opa(btn_network, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(btn_network, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(btn_network, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(btn_network, 18, 0);
  lv_obj_add_event_cb(btn_network, on_network_btn_clicked, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *label_network = lv_label_create(btn_network);
  lv_label_set_text(label_network, LV_SYMBOL_WIFI "\n\nNetzwerk");
  lv_obj_set_style_text_font(label_network, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_align(label_network, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(label_network);

  // Button 2: Display (Position 1,0)
  lv_obj_t *btn_display = lv_button_create(main_menu);
  lv_obj_set_grid_cell(btn_display, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
  lv_obj_set_style_bg_color(btn_display, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_border_opa(btn_display, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(btn_display, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(btn_display, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(btn_display, 18, 0);
  lv_obj_add_event_cb(btn_display, on_display_btn_clicked, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *label_display = lv_label_create(btn_display);
  lv_label_set_text(label_display, LV_SYMBOL_IMAGE "\n\nDisplay");
  lv_obj_set_style_text_font(label_display, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_align(label_display, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(label_display);

  // Button 3: Energie (Position 0,1)
  lv_obj_t *btn_energy = lv_button_create(main_menu);
  lv_obj_set_grid_cell(btn_energy, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
  lv_obj_set_style_bg_color(btn_energy, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_border_opa(btn_energy, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(btn_energy, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(btn_energy, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(btn_energy, 18, 0);
  lv_obj_add_event_cb(btn_energy, on_energy_btn_clicked, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *label_energy = lv_label_create(btn_energy);
  lv_label_set_text(label_energy, LV_SYMBOL_BATTERY_FULL "\n\nEnergie");
  lv_obj_set_style_text_font(label_energy, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_align(label_energy, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(label_energy);
}

// ========== Network Page ==========
static void create_network_page(lv_obj_t *parent) {
  network_page = lv_obj_create(parent);
  lv_obj_set_size(network_page, LV_PCT(100), LV_PCT(100));
  lv_obj_set_scroll_dir(network_page, LV_DIR_VER);
  lv_obj_set_style_bg_color(network_page, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(network_page, LV_OPA_COVER, 0);
  lv_obj_set_style_border_opa(network_page, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(network_page, 20, 0);
  lv_obj_add_flag(network_page, LV_OBJ_FLAG_HIDDEN);

  // Zurück Button
  lv_obj_t *back_btn = lv_button_create(network_page);
  lv_obj_set_size(back_btn, 200, 70);
  lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_border_opa(back_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(back_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(back_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(back_btn, 18, 0);
  lv_obj_add_event_cb(back_btn, on_back_btn_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *back_label = lv_label_create(back_btn);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT);
  lv_obj_set_style_text_font(back_label, &lv_font_montserrat_48, 0);
  lv_obj_center(back_label);

  // Content Container
  lv_obj_t *content = lv_obj_create(network_page);
  lv_obj_set_size(content, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 90);
  lv_obj_set_style_bg_color(content, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_border_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(content, 22, 0);
  lv_obj_set_style_pad_all(content, 20, 0);

  // Title
  lv_obj_t *title = lv_label_create(content);
  lv_label_set_text(title, LV_SYMBOL_WIFI " WLAN-Einstellungen");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  // Status
  wifi_status_label = lv_label_create(content);
  lv_label_set_text(wifi_status_label, "Status: " LV_SYMBOL_CLOSE " Nicht verbunden");
  lv_obj_set_style_text_font(wifi_status_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFF6B6B), 0);
  lv_obj_align(wifi_status_label, LV_ALIGN_TOP_LEFT, 0, 40);

  // SSID
  wifi_ssid_label = lv_label_create(content);
  lv_label_set_text(wifi_ssid_label, "Netzwerk: ---");
  lv_obj_set_style_text_font(wifi_ssid_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(wifi_ssid_label, lv_color_hex(0xC8C8C8), 0);
  lv_obj_align(wifi_ssid_label, LV_ALIGN_TOP_LEFT, 0, 70);

  // IP
  wifi_ip_label = lv_label_create(content);
  lv_label_set_text(wifi_ip_label, "IP-Adresse: ---");
  lv_obj_set_style_text_font(wifi_ip_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(wifi_ip_label, lv_color_hex(0xC8C8C8), 0);
  lv_obj_align(wifi_ip_label, LV_ALIGN_TOP_LEFT, 0, 95);

  // MQTT Warning
  mqtt_notice_label = lv_label_create(content);
  lv_label_set_text(mqtt_notice_label,
                    LV_SYMBOL_WARNING " MQTT nicht konfiguriert\n"
                    "Oeffne das Web-Interface und trage Host & Zugangsdaten ein.");
  lv_obj_set_width(mqtt_notice_label, LV_PCT(100));
  lv_obj_set_style_text_font(mqtt_notice_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(mqtt_notice_label, lv_color_hex(0xFFC04D), 0);
  lv_obj_align(mqtt_notice_label, LV_ALIGN_TOP_LEFT, 0, 125);
  lv_obj_add_flag(mqtt_notice_label, LV_OBJ_FLAG_HIDDEN);

  // AP Mode Button
  ap_mode_btn = lv_button_create(content);
  lv_obj_set_size(ap_mode_btn, LV_PCT(100), 120);
  lv_obj_align(ap_mode_btn, LV_ALIGN_TOP_LEFT, 0, 200);
  lv_obj_set_style_bg_color(ap_mode_btn, lv_color_hex(0xFF9800), 0);
  lv_obj_set_style_border_opa(ap_mode_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(ap_mode_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(ap_mode_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(ap_mode_btn, 18, 0);
  lv_obj_add_event_cb(ap_mode_btn, on_ap_mode_clicked, LV_EVENT_CLICKED, nullptr);

  ap_mode_btn_label = lv_label_create(ap_mode_btn);
  lv_label_set_text(ap_mode_btn_label, LV_SYMBOL_SETTINGS " AP-Modus aktivieren");
  lv_obj_set_style_text_font(ap_mode_btn_label, &lv_font_montserrat_20, 0);
  lv_obj_center(ap_mode_btn_label);

  // Bestätigungs-Panel (initial versteckt)
  confirm_panel = lv_obj_create(network_page);
  lv_obj_set_size(confirm_panel, 600, 300);
  lv_obj_center(confirm_panel);
  lv_obj_set_style_bg_color(confirm_panel, lv_color_hex(0xE57373), 0);  // Pastel Rot
  lv_obj_set_style_border_opa(confirm_panel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(confirm_panel, 22, 0);
  lv_obj_set_style_pad_all(confirm_panel, 30, 0);
  lv_obj_add_flag(confirm_panel, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *confirm_title = lv_label_create(confirm_panel);
  lv_label_set_text(confirm_title, LV_SYMBOL_WARNING " AP-Modus aktivieren?");
  lv_obj_set_style_text_font(confirm_title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(confirm_title, lv_color_hex(0x1A1A1A), 0);  // Dunkel
  lv_obj_align(confirm_title, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t *confirm_text = lv_label_create(confirm_panel);
  lv_label_set_text(confirm_text, "WiFi wird getrennt!");
  lv_obj_set_style_text_font(confirm_text, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(confirm_text, lv_color_white(), 0);  // Weiß
  lv_obj_align(confirm_text, LV_ALIGN_TOP_MID, 0, 50);

  confirm_yes_btn = lv_button_create(confirm_panel);
  lv_obj_set_size(confirm_yes_btn, 200, 120);
  lv_obj_align(confirm_yes_btn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_color(confirm_yes_btn, lv_color_hex(0xC62828), 0);  // Dunkelrot
  lv_obj_set_style_border_opa(confirm_yes_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(confirm_yes_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(confirm_yes_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(confirm_yes_btn, 18, 0);
  lv_obj_add_event_cb(confirm_yes_btn, on_confirm_yes_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *yes_label = lv_label_create(confirm_yes_btn);
  lv_label_set_text(yes_label, "Aktivieren");
  lv_obj_set_style_text_font(yes_label, &lv_font_montserrat_20, 0);
  lv_obj_center(yes_label);

  confirm_no_btn = lv_button_create(confirm_panel);
  lv_obj_set_size(confirm_no_btn, 200, 120);
  lv_obj_align(confirm_no_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_set_style_bg_color(confirm_no_btn, lv_color_hex(0x555555), 0);
  lv_obj_set_style_border_opa(confirm_no_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(confirm_no_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(confirm_no_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(confirm_no_btn, 18, 0);
  lv_obj_add_event_cb(confirm_no_btn, on_confirm_no_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *no_label = lv_label_create(confirm_no_btn);
  lv_label_set_text(no_label, "Abbrechen");
  lv_obj_set_style_text_font(no_label, &lv_font_montserrat_20, 0);
  lv_obj_center(no_label);
}

// ========== Display Page ==========
static void create_display_page(lv_obj_t *parent) {
  display_page = lv_obj_create(parent);
  lv_obj_set_size(display_page, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(display_page, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(display_page, LV_OPA_COVER, 0);
  lv_obj_set_style_border_opa(display_page, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(display_page, 20, 0);
  lv_obj_add_flag(display_page, LV_OBJ_FLAG_HIDDEN);

  // Zurück Button
  lv_obj_t *back_btn = lv_button_create(display_page);
  lv_obj_set_size(back_btn, 200, 70);
  lv_obj_set_style_border_opa(back_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(back_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(back_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(back_btn, 18, 0);
  lv_obj_add_event_cb(back_btn, on_back_btn_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *back_label = lv_label_create(back_btn);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT);
  lv_obj_set_style_text_font(back_label, &lv_font_montserrat_48, 0);
  lv_obj_center(back_label);

  // Content
  lv_obj_t *content = lv_obj_create(display_page);
  lv_obj_set_size(content, LV_PCT(100), 200);
  lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 90);
  lv_obj_set_style_bg_color(content, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_border_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(content, 22, 0);
  lv_obj_set_style_pad_all(content, 20, 0);

  // Title
  lv_obj_t *title = lv_label_create(content);
  lv_label_set_text(title, LV_SYMBOL_IMAGE " Helligkeit");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  // Helligkeit: Label + Slider + Wert in einer Zeile
  lv_obj_t *brightness_container = lv_label_create(content);
  lv_label_set_text(brightness_container, "Helligkeit:");
  lv_obj_set_style_text_color(brightness_container, lv_color_white(), 0);
  lv_obj_set_style_text_font(brightness_container, &lv_font_montserrat_24, 0);
  lv_obj_align(brightness_container, LV_ALIGN_TOP_LEFT, 0, 60);

  // Standard Slider
  lv_obj_t *slider = lv_slider_create(content);
  lv_obj_set_size(slider, 400, 20);
  lv_obj_align(slider, LV_ALIGN_TOP_LEFT, 150, 65);
  lv_slider_set_range(slider, 75, 255);
  int current_brightness = M5.Display.getBrightness();
  lv_slider_set_value(slider, current_brightness, LV_ANIM_OFF);
  lv_obj_add_event_cb(slider, on_brightness, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(slider, on_brightness, LV_EVENT_RELEASED, nullptr);

  // Value Label (rechts)
  brightness_label = lv_label_create(content);
  static char bright_buf[16];
  snprintf(bright_buf, sizeof(bright_buf), "%d", current_brightness);
  lv_label_set_text(brightness_label, bright_buf);
  lv_obj_set_style_text_color(brightness_label, lv_color_white(), 0);
  lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_24, 0);
  lv_obj_align(brightness_label, LV_ALIGN_TOP_LEFT, 570, 60);
}

// ========== Energy Page ==========
static void create_energy_page(lv_obj_t *parent) {
  energy_page = lv_obj_create(parent);
  lv_obj_set_size(energy_page, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(energy_page, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(energy_page, LV_OPA_COVER, 0);
  lv_obj_set_style_border_opa(energy_page, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(energy_page, 20, 0);
  lv_obj_add_flag(energy_page, LV_OBJ_FLAG_HIDDEN);

  // Zurück Button
  lv_obj_t *back_btn = lv_button_create(energy_page);
  lv_obj_set_size(back_btn, 200, 70);
  lv_obj_set_style_border_opa(back_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(back_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(back_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(back_btn, 18, 0);
  lv_obj_add_event_cb(back_btn, on_back_btn_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *back_label = lv_label_create(back_btn);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT);
  lv_obj_set_style_text_font(back_label, &lv_font_montserrat_48, 0);
  lv_obj_center(back_label);

  // Content
  lv_obj_t *content = lv_obj_create(energy_page);
  lv_obj_set_size(content, LV_PCT(100), 580);  // Mehr Höhe für alle Elemente
  lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 90);
  lv_obj_set_style_bg_color(content, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_border_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(content, 22, 0);
  lv_obj_set_style_pad_all(content, 20, 0);

  // Netzteilbetrieb
  lv_obj_t *mains_title = lv_label_create(content);
  lv_label_set_text(mains_title, LV_SYMBOL_POWER " Netzteilbetrieb");
  lv_obj_set_style_text_font(mains_title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(mains_title, lv_color_white(), 0);
  lv_obj_align(mains_title, LV_ALIGN_TOP_LEFT, 0, 0);

  const DeviceConfig& cfg = configManager.getConfig();

  // Checkbox (größer + weiße Schrift)
  sleep_checkbox = lv_checkbox_create(content);
  lv_checkbox_set_text(sleep_checkbox, "Auto-Sleep aktiviert");
  lv_obj_set_style_text_font(sleep_checkbox, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(sleep_checkbox, lv_color_white(), 0);  // Weiß!
  lv_obj_set_style_pad_column(sleep_checkbox, 20, 0);  // Abstand zwischen Checkbox und Text
  lv_obj_set_width(sleep_checkbox, LV_SIZE_CONTENT);
  lv_obj_set_height(sleep_checkbox, LV_SIZE_CONTENT);
  lv_obj_align(sleep_checkbox, LV_ALIGN_TOP_LEFT, 0, 40);

  // Checkbox-Indicator richtig groß machen (100x100)
  lv_obj_set_style_width(sleep_checkbox, 100, LV_PART_INDICATOR);
  lv_obj_set_style_height(sleep_checkbox, 100, LV_PART_INDICATOR);

  if (cfg.auto_sleep_enabled) {
    lv_obj_add_state(sleep_checkbox, LV_STATE_CHECKED);
  }
  lv_obj_add_event_cb(sleep_checkbox, on_sleep_checkbox, LV_EVENT_VALUE_CHANGED, nullptr);

  // Sleep nach: Label + Slider + Wert in einer Zeile
  sleep_label = lv_label_create(content);
  lv_label_set_text(sleep_label, "Sleep nach:");
  lv_obj_set_style_text_color(sleep_label, lv_color_white(), 0);
  lv_obj_set_style_text_font(sleep_label, &lv_font_montserrat_24, 0);
  lv_obj_align(sleep_label, LV_ALIGN_TOP_LEFT, 0, 110);

  // Standard Slider
  sleep_slider = lv_slider_create(content);
  lv_obj_set_size(sleep_slider, 400, 20);
  lv_obj_align(sleep_slider, LV_ALIGN_TOP_LEFT, 150, 115);
  lv_slider_set_range(sleep_slider, 0, static_cast<int32_t>(kSleepOptionsSecCount) - 1);
  int32_t sleep_index = sleep_index_from_seconds(cfg.auto_sleep_seconds);
  uint16_t sleep_seconds = sleep_seconds_from_index(sleep_index);
  lv_slider_set_value(sleep_slider, sleep_index, LV_ANIM_OFF);
  lv_obj_add_event_cb(sleep_slider, on_sleep_slider, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(sleep_slider, on_sleep_slider, LV_EVENT_RELEASED, nullptr);

  // Value Label (rechts)
  sleep_time_label = lv_label_create(content);
  static char sleep_buf[16];
  format_sleep_label(sleep_buf, sizeof(sleep_buf), sleep_seconds);
  lv_label_set_text(sleep_time_label, sleep_buf);
  lv_obj_set_style_text_color(sleep_time_label, lv_color_white(), 0);
  lv_obj_set_style_text_font(sleep_time_label, &lv_font_montserrat_24, 0);
  lv_obj_align(sleep_time_label, LV_ALIGN_TOP_LEFT, 570, 110);

  // Verstecke Slider + Labels wenn deaktiviert
  if (!cfg.auto_sleep_enabled) {
    lv_obj_add_flag(sleep_slider, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sleep_time_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sleep_label, LV_OBJ_FLAG_HIDDEN);
  }

  // Batteriebetrieb
  lv_obj_t *battery_title = lv_label_create(content);
  lv_label_set_text(battery_title, LV_SYMBOL_BATTERY_FULL " Batteriebetrieb");
  lv_obj_set_style_text_font(battery_title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(battery_title, lv_color_white(), 0);
  lv_obj_align(battery_title, LV_ALIGN_TOP_LEFT, 0, 180);

  sleep_battery_checkbox = lv_checkbox_create(content);
  lv_checkbox_set_text(sleep_battery_checkbox, "Auto-Sleep aktiviert");
  lv_obj_set_style_text_font(sleep_battery_checkbox, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(sleep_battery_checkbox, lv_color_white(), 0);
  lv_obj_set_style_pad_column(sleep_battery_checkbox, 20, 0);
  lv_obj_set_width(sleep_battery_checkbox, LV_SIZE_CONTENT);
  lv_obj_set_height(sleep_battery_checkbox, LV_SIZE_CONTENT);
  lv_obj_align(sleep_battery_checkbox, LV_ALIGN_TOP_LEFT, 0, 220);

  lv_obj_set_style_width(sleep_battery_checkbox, 100, LV_PART_INDICATOR);
  lv_obj_set_style_height(sleep_battery_checkbox, 100, LV_PART_INDICATOR);

  if (cfg.auto_sleep_battery_enabled) {
    lv_obj_add_state(sleep_battery_checkbox, LV_STATE_CHECKED);
  }
  lv_obj_add_event_cb(sleep_battery_checkbox, on_sleep_battery_checkbox, LV_EVENT_VALUE_CHANGED, nullptr);

  sleep_battery_label = lv_label_create(content);
  lv_label_set_text(sleep_battery_label, "Sleep nach:");
  lv_obj_set_style_text_color(sleep_battery_label, lv_color_white(), 0);
  lv_obj_set_style_text_font(sleep_battery_label, &lv_font_montserrat_24, 0);
  lv_obj_align(sleep_battery_label, LV_ALIGN_TOP_LEFT, 0, 290);

  sleep_battery_slider = lv_slider_create(content);
  lv_obj_set_size(sleep_battery_slider, 400, 20);
  lv_obj_align(sleep_battery_slider, LV_ALIGN_TOP_LEFT, 150, 295);
  lv_slider_set_range(sleep_battery_slider, 0, static_cast<int32_t>(kSleepOptionsSecCount) - 1);
  int32_t sleep_battery_index = sleep_index_from_seconds(cfg.auto_sleep_battery_seconds);
  uint16_t sleep_battery_seconds = sleep_seconds_from_index(sleep_battery_index);
  lv_slider_set_value(sleep_battery_slider, sleep_battery_index, LV_ANIM_OFF);
  lv_obj_add_event_cb(sleep_battery_slider, on_sleep_battery_slider, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(sleep_battery_slider, on_sleep_battery_slider, LV_EVENT_RELEASED, nullptr);

  sleep_battery_time_label = lv_label_create(content);
  static char sleep_battery_buf[16];
  format_sleep_label(sleep_battery_buf, sizeof(sleep_battery_buf), sleep_battery_seconds);
  lv_label_set_text(sleep_battery_time_label, sleep_battery_buf);
  lv_obj_set_style_text_color(sleep_battery_time_label, lv_color_white(), 0);
  lv_obj_set_style_text_font(sleep_battery_time_label, &lv_font_montserrat_24, 0);
  lv_obj_align(sleep_battery_time_label, LV_ALIGN_TOP_LEFT, 570, 290);

  if (!cfg.auto_sleep_battery_enabled) {
    lv_obj_add_flag(sleep_battery_slider, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sleep_battery_time_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sleep_battery_label, LV_OBJ_FLAG_HIDDEN);
  }

  // Power Status Anzeige (kompakt)
  lv_obj_t *power_title = lv_label_create(content);
  lv_label_set_text(power_title, LV_SYMBOL_CHARGE " Batterie-Status");
  lv_obj_set_style_text_font(power_title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(power_title, lv_color_hex(0xFFA726), 0);
  lv_obj_align(power_title, LV_ALIGN_TOP_LEFT, 0, 360);

  // Status Labels (direkt im content, nicht in eigenem Container)
  power_status_label = lv_label_create(content);
  lv_obj_set_style_text_font(power_status_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(power_status_label, lv_color_white(), 0);
  lv_obj_align(power_status_label, LV_ALIGN_TOP_LEFT, 0, 400);

  power_voltage_label = lv_label_create(content);
  lv_obj_set_style_text_font(power_voltage_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(power_voltage_label, lv_color_white(), 0);
  lv_obj_align(power_voltage_label, LV_ALIGN_TOP_LEFT, 0, 430);

  power_level_label = lv_label_create(content);
  lv_obj_set_style_text_font(power_level_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(power_level_label, lv_color_white(), 0);
  lv_obj_align(power_level_label, LV_ALIGN_TOP_LEFT, 0, 460);

  // Initial update
  settings_update_power_status();
}

// ========== Page Navigation ==========
static void show_main_menu() {
  if (main_menu) lv_obj_clear_flag(main_menu, LV_OBJ_FLAG_HIDDEN);
  if (network_page) lv_obj_add_flag(network_page, LV_OBJ_FLAG_HIDDEN);
  if (display_page) lv_obj_add_flag(display_page, LV_OBJ_FLAG_HIDDEN);
  if (energy_page) lv_obj_add_flag(energy_page, LV_OBJ_FLAG_HIDDEN);
}

static void show_network_page() {
  if (main_menu) lv_obj_add_flag(main_menu, LV_OBJ_FLAG_HIDDEN);
  if (network_page) lv_obj_clear_flag(network_page, LV_OBJ_FLAG_HIDDEN);
  if (display_page) lv_obj_add_flag(display_page, LV_OBJ_FLAG_HIDDEN);
  if (energy_page) lv_obj_add_flag(energy_page, LV_OBJ_FLAG_HIDDEN);
}

static void show_display_page() {
  if (main_menu) lv_obj_add_flag(main_menu, LV_OBJ_FLAG_HIDDEN);
  if (network_page) lv_obj_add_flag(network_page, LV_OBJ_FLAG_HIDDEN);
  if (display_page) lv_obj_clear_flag(display_page, LV_OBJ_FLAG_HIDDEN);
  if (energy_page) lv_obj_add_flag(energy_page, LV_OBJ_FLAG_HIDDEN);
}

static void show_energy_page() {
  if (main_menu) lv_obj_add_flag(main_menu, LV_OBJ_FLAG_HIDDEN);
  if (network_page) lv_obj_add_flag(network_page, LV_OBJ_FLAG_HIDDEN);
  if (display_page) lv_obj_add_flag(display_page, LV_OBJ_FLAG_HIDDEN);
  if (energy_page) lv_obj_clear_flag(energy_page, LV_OBJ_FLAG_HIDDEN);
}

// ========== Public API ==========
void build_settings_tab(lv_obj_t *tab, hotspot_callback_t hotspot_cb) {
  g_hotspot_callback = hotspot_cb;
  ap_mode_confirm_pending = false;

  lv_obj_clean(tab);
  lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(tab, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
  lv_obj_set_style_border_opa(tab, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(tab, 24, 0);

  static int32_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static int32_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(tab, col_dsc, row_dsc);
  lv_obj_set_style_pad_column(tab, 24, 0);
  lv_obj_set_style_pad_row(tab, 24, 0);

  const DeviceConfig& cfg = configManager.getConfig();

  // Display tile
  lv_obj_t *card_display = create_settings_card(tab, 0, 0);
  lv_obj_set_layout(card_display, LV_LAYOUT_NONE);
  lv_obj_t *title = lv_label_create(card_display);
  lv_label_set_text(title, LV_SYMBOL_IMAGE " Display");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, kSettingsPadX, kSettingsTitleY);

  lv_obj_t *label = lv_label_create(card_display);
  lv_label_set_text(label, "Helligkeit");
  lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, kSettingsPadX, kSettingsRowY);

  brightness_label = lv_label_create(card_display);
  lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(brightness_label, lv_color_white(), 0);
  lv_obj_align(brightness_label, LV_ALIGN_TOP_RIGHT, -kSettingsPadX, kSettingsRowY);

  lv_obj_t *brightness_slider = lv_slider_create(card_display);
  style_settings_slider(brightness_slider);
  lv_obj_align(brightness_slider, LV_ALIGN_TOP_MID, 0, kSettingsSliderY);
  lv_slider_set_range(brightness_slider, 75, 255);
  int current_brightness = M5.Display.getBrightness();
  lv_slider_set_value(brightness_slider, current_brightness, LV_ANIM_OFF);
  lv_obj_add_event_cb(brightness_slider, on_brightness, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(brightness_slider, on_brightness, LV_EVENT_RELEASED, nullptr);

  static char bright_buf[16];
  snprintf(bright_buf, sizeof(bright_buf), "%d", current_brightness);
  lv_label_set_text(brightness_label, bright_buf);

  // WiFi tile
  lv_obj_t *card_wifi = create_settings_card(tab, 0, 1);
  title = lv_label_create(card_wifi);
  lv_label_set_text(title, LV_SYMBOL_WIFI " WLAN");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);

  wifi_status_label = lv_label_create(card_wifi);
  lv_label_set_text(wifi_status_label, "Status: " LV_SYMBOL_CLOSE " Nicht verbunden");
  lv_obj_set_style_text_font(wifi_status_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFF6B6B), 0);

  wifi_ssid_label = lv_label_create(card_wifi);
  lv_label_set_text(wifi_ssid_label, "Netzwerk: ---");
  lv_obj_set_style_text_font(wifi_ssid_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(wifi_ssid_label, lv_color_hex(0xC8C8C8), 0);

  wifi_ip_label = lv_label_create(card_wifi);
  lv_label_set_text(wifi_ip_label, "IP-Adresse: ---");
  lv_obj_set_style_text_font(wifi_ip_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(wifi_ip_label, lv_color_hex(0xC8C8C8), 0);

  mqtt_notice_label = lv_label_create(card_wifi);
  lv_label_set_text(mqtt_notice_label,
                    LV_SYMBOL_WARNING " MQTT nicht konfiguriert\n"
                    "Web-Interface oeffnen und Zugangsdaten setzen.");
  lv_obj_set_width(mqtt_notice_label, LV_PCT(100));
  lv_label_set_long_mode(mqtt_notice_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(mqtt_notice_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(mqtt_notice_label, lv_color_hex(0xFFC04D), 0);
  lv_obj_add_flag(mqtt_notice_label, LV_OBJ_FLAG_HIDDEN);

  ap_mode_btn = lv_button_create(card_wifi);
  lv_obj_set_width(ap_mode_btn, LV_PCT(100));
  lv_obj_set_height(ap_mode_btn, 60);
  lv_obj_set_style_bg_color(ap_mode_btn, lv_color_hex(0xFF9800), 0);
  lv_obj_set_style_border_opa(ap_mode_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(ap_mode_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(ap_mode_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(ap_mode_btn, 18, 0);
  lv_obj_add_event_cb(ap_mode_btn, on_ap_mode_clicked, LV_EVENT_CLICKED, nullptr);

  ap_mode_btn_label = lv_label_create(ap_mode_btn);
  lv_label_set_text(ap_mode_btn_label, "AP aktivieren");
  lv_obj_set_style_text_font(ap_mode_btn_label, &lv_font_montserrat_20, 0);
  lv_obj_center(ap_mode_btn_label);

  ap_confirm_row = lv_obj_create(card_wifi);
  lv_obj_set_width(ap_confirm_row, LV_PCT(100));
  lv_obj_set_style_bg_opa(ap_confirm_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(ap_confirm_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(ap_confirm_row, 0, 0);
  lv_obj_set_style_pad_column(ap_confirm_row, 12, 0);
  lv_obj_set_flex_flow(ap_confirm_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(ap_confirm_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(ap_confirm_row, LV_OBJ_FLAG_HIDDEN);

  ap_confirm_yes_btn = lv_button_create(ap_confirm_row);
  lv_obj_set_height(ap_confirm_yes_btn, 50);
  lv_obj_set_flex_grow(ap_confirm_yes_btn, 1);
  lv_obj_set_style_bg_color(ap_confirm_yes_btn, lv_color_hex(0xC62828), 0);
  lv_obj_set_style_border_opa(ap_confirm_yes_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(ap_confirm_yes_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(ap_confirm_yes_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(ap_confirm_yes_btn, 18, 0);
  lv_obj_add_event_cb(ap_confirm_yes_btn, on_confirm_yes_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *ap_yes_label = lv_label_create(ap_confirm_yes_btn);
  lv_label_set_text(ap_yes_label, "Bestaetigen");
  lv_obj_set_style_text_font(ap_yes_label, &lv_font_montserrat_20, 0);
  lv_obj_center(ap_yes_label);

  ap_confirm_no_btn = lv_button_create(ap_confirm_row);
  lv_obj_set_height(ap_confirm_no_btn, 50);
  lv_obj_set_flex_grow(ap_confirm_no_btn, 1);
  lv_obj_set_style_bg_color(ap_confirm_no_btn, lv_color_hex(0x555555), 0);
  lv_obj_set_style_border_opa(ap_confirm_no_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(ap_confirm_no_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(ap_confirm_no_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(ap_confirm_no_btn, 18, 0);
  lv_obj_add_event_cb(ap_confirm_no_btn, on_confirm_no_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *ap_no_label = lv_label_create(ap_confirm_no_btn);
  lv_label_set_text(ap_no_label, "Abbrechen");
  lv_obj_set_style_text_font(ap_no_label, &lv_font_montserrat_20, 0);
  lv_obj_center(ap_no_label);

  // Sleep mains tile
  lv_obj_t *card_mains = create_settings_card(tab, 1, 0);
  lv_obj_set_layout(card_mains, LV_LAYOUT_NONE);
  title = lv_label_create(card_mains);
  lv_label_set_text(title, LV_SYMBOL_POWER " Sleep Netzteil");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, kSettingsPadX, kSettingsTitleY);

  label = lv_label_create(card_mains);
  lv_label_set_text(label, "Auto-Sleep nach");
  lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, kSettingsPadX, kSettingsRowY);

  sleep_time_label = lv_label_create(card_mains);
  lv_obj_set_style_text_font(sleep_time_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(sleep_time_label, lv_color_white(), 0);
  lv_obj_align(sleep_time_label, LV_ALIGN_TOP_RIGHT, -kSettingsPadX, kSettingsRowY);

  sleep_slider = lv_slider_create(card_mains);
  style_settings_slider(sleep_slider);
  lv_obj_align(sleep_slider, LV_ALIGN_TOP_MID, 0, kSettingsSliderY);
  lv_slider_set_range(sleep_slider, 0, sleep_slider_max_index());
  int32_t sleep_index = cfg.auto_sleep_enabled
                        ? sleep_index_from_seconds(cfg.auto_sleep_seconds)
                        : sleep_slider_max_index();
  lv_slider_set_value(sleep_slider, sleep_index, LV_ANIM_OFF);
  lv_obj_add_event_cb(sleep_slider, on_sleep_slider, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(sleep_slider, on_sleep_slider, LV_EVENT_RELEASED, nullptr);

  static char sleep_buf[16];
  format_sleep_label_for_index(sleep_buf, sizeof(sleep_buf), sleep_index);
  lv_label_set_text(sleep_time_label, sleep_buf);

  // Sleep battery tile
  lv_obj_t *card_battery = create_settings_card(tab, 1, 1);
  lv_obj_set_layout(card_battery, LV_LAYOUT_NONE);
  title = lv_label_create(card_battery);
  lv_label_set_text(title, LV_SYMBOL_BATTERY_FULL " Sleep Batterie");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, kSettingsPadX, kSettingsTitleY);

  label = lv_label_create(card_battery);
  lv_label_set_text(label, "Auto-Sleep nach");
  lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, kSettingsPadX, kSettingsRowY);

  sleep_battery_time_label = lv_label_create(card_battery);
  lv_obj_set_style_text_font(sleep_battery_time_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(sleep_battery_time_label, lv_color_white(), 0);
  lv_obj_align(sleep_battery_time_label, LV_ALIGN_TOP_RIGHT, -kSettingsPadX, kSettingsRowY);

  sleep_battery_slider = lv_slider_create(card_battery);
  style_settings_slider(sleep_battery_slider);
  lv_obj_align(sleep_battery_slider, LV_ALIGN_TOP_MID, 0, kSettingsSliderY);
  lv_slider_set_range(sleep_battery_slider, 0, sleep_slider_max_index());
  int32_t sleep_battery_index = cfg.auto_sleep_battery_enabled
                                ? sleep_index_from_seconds(cfg.auto_sleep_battery_seconds)
                                : sleep_slider_max_index();
  lv_slider_set_value(sleep_battery_slider, sleep_battery_index, LV_ANIM_OFF);
  lv_obj_add_event_cb(sleep_battery_slider, on_sleep_battery_slider, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(sleep_battery_slider, on_sleep_battery_slider, LV_EVENT_RELEASED, nullptr);

  static char sleep_battery_buf[16];
  format_sleep_label_for_index(sleep_battery_buf, sizeof(sleep_battery_buf), sleep_battery_index);
  lv_label_set_text(sleep_battery_time_label, sleep_battery_buf);

  power_status_label = lv_label_create(card_battery);
  lv_label_set_text(power_status_label, "Status: ---");
  lv_obj_set_width(power_status_label, LV_PCT(100));
  lv_label_set_long_mode(power_status_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(power_status_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(power_status_label, lv_color_white(), 0);
  lv_obj_align(power_status_label, LV_ALIGN_TOP_LEFT, kSettingsPadX, kSettingsStatusY);

  power_level_label = lv_label_create(card_battery);
  lv_label_set_text(power_level_label, "Level: --%");
  lv_obj_set_width(power_level_label, LV_PCT(100));
  lv_label_set_long_mode(power_level_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(power_level_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(power_level_label, lv_color_white(), 0);
  lv_obj_align(power_level_label, LV_ALIGN_TOP_LEFT, kSettingsPadX, kSettingsStatusY + kSettingsStatusGap);

  settings_update_power_status();

  settings_update_ap_mode(false);
}

void settings_update_wifi_status(bool connected, const char* ssid, const char* ip) {
  if (!wifi_status_label || !wifi_ssid_label || !wifi_ip_label) return;

  static char buf[128];

  if (connected) {
    lv_label_set_text(wifi_status_label, "Status: " LV_SYMBOL_OK " Verbunden");
    lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0x51CF66), 0);

    snprintf(buf, sizeof(buf), "Netzwerk: %s", ssid ? ssid : "---");
    lv_label_set_text(wifi_ssid_label, buf);

    snprintf(buf, sizeof(buf), "IP-Adresse: %s", ip ? ip : "---");
    lv_label_set_text(wifi_ip_label, buf);

  } else {
    lv_label_set_text(wifi_status_label, "Status: " LV_SYMBOL_CLOSE " Nicht verbunden");
    lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFF6B6B), 0);
    lv_label_set_text(wifi_ssid_label, "Netzwerk: ---");
    lv_label_set_text(wifi_ip_label, "IP-Adresse: ---");
  }
}

void settings_show_mqtt_warning(bool show) {
  if (!mqtt_notice_label) return;
  if (show) {
    lv_obj_clear_flag(mqtt_notice_label, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(mqtt_notice_label, LV_OBJ_FLAG_HIDDEN);
  }
}

void settings_update_ap_mode(bool running) {
  ap_mode_active = running;
  if (ap_mode_btn_label) {
    lv_label_set_text(ap_mode_btn_label, running ? "AP beenden" : "AP aktivieren");
  }
  if (ap_mode_btn) {
    lv_obj_set_style_bg_color(ap_mode_btn,
                              running ? lv_color_hex(0xC62828) : lv_color_hex(0xFF9800),
                              0);
    lv_obj_clear_flag(ap_mode_btn, LV_OBJ_FLAG_HIDDEN);
  }
  if (running) {
    hide_confirm_panel();
  }
}
