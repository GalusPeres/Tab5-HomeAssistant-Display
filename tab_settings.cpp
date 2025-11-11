#include <M5Unified.h>
#include <lvgl.h>
#include "tab_settings.h"

static lv_obj_t *brightness_label = nullptr;
static hotspot_callback_t g_hotspot_callback = nullptr;

// WiFi Status Labels
static lv_obj_t *wifi_status_label = nullptr;
static lv_obj_t *wifi_ssid_label = nullptr;
static lv_obj_t *wifi_ip_label = nullptr;
static lv_obj_t *ap_mode_btn = nullptr;
static lv_obj_t *mqtt_notice_label = nullptr;
static lv_obj_t *msgbox = nullptr;

static void on_brightness(lv_event_t *e) {
  lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
  int32_t value = lv_slider_get_value(slider);
  M5.Display.setBrightness(value);

  static char buf[32];
  snprintf(buf, sizeof(buf), "Helligkeit: %d", (int)value);
  if (brightness_label) lv_label_set_text(brightness_label, buf);
}

// Bestätigungsdialog Button Event Handler
static void msgbox_btn_event_handler(lv_event_t *e) {
  lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
  lv_obj_t *parent = lv_obj_get_parent(btn);
  lv_obj_t *mb = lv_obj_get_parent(parent);  // msgbox ist Großeltern-Objekt

  // Prüfe welcher Button geklickt wurde (erster Button = Ja)
  if (btn == lv_obj_get_child(parent, 0)) {
    // "Ja" Button wurde geklickt
    if (g_hotspot_callback) {
      g_hotspot_callback();
    }
  }

  // Dialog schließen
  lv_msgbox_close(mb);
  msgbox = nullptr;
}

static void on_ap_mode_clicked(lv_event_t *e) {
  // Bestätigungsdialog anzeigen
  if (msgbox) return;  // Bereits ein Dialog offen

  msgbox = lv_msgbox_create(lv_screen_active());

  // LVGL v9 API: Titel, Text und Buttons separat hinzufügen
  lv_msgbox_add_title(msgbox, LV_SYMBOL_WARNING " AP-Modus aktivieren?");
  lv_msgbox_add_text(msgbox,
    "WiFi-Verbindung wird getrennt!\n\n"
    "Hotspot: Tab5_Config\n"
    "Passwort: 12345678\n"
    "IP: 192.168.4.1\n\n"
    "Wirklich aktivieren?");

  // Buttons hinzufügen
  lv_obj_t *btn_yes = lv_msgbox_add_footer_button(msgbox, "Ja, starten");
  lv_obj_t *btn_no = lv_msgbox_add_footer_button(msgbox, "Abbrechen");

  // Event Callbacks für beide Buttons
  lv_obj_add_event_cb(btn_yes, msgbox_btn_event_handler, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(btn_no, msgbox_btn_event_handler, LV_EVENT_CLICKED, nullptr);

  lv_obj_center(msgbox);

  // Style
  lv_obj_set_style_bg_color(msgbox, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_text_color(msgbox, lv_color_white(), 0);
}

void build_settings_tab(lv_obj_t *tab, hotspot_callback_t hotspot_cb) {
  g_hotspot_callback = hotspot_cb;

  // Scrollbares Container
  lv_obj_set_scroll_dir(tab, LV_DIR_VER);
  lv_obj_set_style_bg_color(tab, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(tab, 20, 0);

  lv_obj_t *container = lv_obj_create(tab);
  lv_obj_set_size(container, 1050, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(container, 0, 0);
  lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(container, 20, 0);

  // ========== WiFi Status Card ==========
  lv_obj_t *wifi_card = lv_obj_create(container);
  lv_obj_set_size(wifi_card, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(wifi_card, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_border_opa(wifi_card, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(wifi_card, 12, 0);
  lv_obj_set_style_pad_all(wifi_card, 20, 0);

  // Card Title
  lv_obj_t *wifi_title = lv_label_create(wifi_card);
  lv_label_set_text(wifi_title, LV_SYMBOL_WIFI " WLAN-Einstellungen");
  lv_obj_set_style_text_font(wifi_title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(wifi_title, lv_color_white(), 0);
  lv_obj_align(wifi_title, LV_ALIGN_TOP_LEFT, 0, 0);

  // Status Label
  wifi_status_label = lv_label_create(wifi_card);
  lv_label_set_text(wifi_status_label, "Status: " LV_SYMBOL_CLOSE " Nicht verbunden");
  lv_obj_set_style_text_font(wifi_status_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFF6B6B), 0);
  lv_obj_align(wifi_status_label, LV_ALIGN_TOP_LEFT, 0, 40);

  // SSID Label
  wifi_ssid_label = lv_label_create(wifi_card);
  lv_label_set_text(wifi_ssid_label, "Netzwerk: ---");
  lv_obj_set_style_text_font(wifi_ssid_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(wifi_ssid_label, lv_color_hex(0xC8C8C8), 0);
  lv_obj_align(wifi_ssid_label, LV_ALIGN_TOP_LEFT, 0, 70);

  // IP Label
  wifi_ip_label = lv_label_create(wifi_card);
  lv_label_set_text(wifi_ip_label, "IP-Adresse: ---");
  lv_obj_set_style_text_font(wifi_ip_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(wifi_ip_label, lv_color_hex(0xC8C8C8), 0);
  lv_obj_align(wifi_ip_label, LV_ALIGN_TOP_LEFT, 0, 95);

  mqtt_notice_label = lv_label_create(wifi_card);
  lv_label_set_text(mqtt_notice_label,
                    LV_SYMBOL_WARNING " MQTT nicht konfiguriert\n"
                    "Oeffne das Web-Interface und trage Host & Zugangsdaten ein.");
  lv_obj_set_width(mqtt_notice_label, LV_PCT(100));
  lv_obj_set_style_text_font(mqtt_notice_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(mqtt_notice_label, lv_color_hex(0xFFC04D), 0);
  lv_obj_set_style_text_align(mqtt_notice_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_align(mqtt_notice_label, LV_ALIGN_TOP_LEFT, 0, 125);
  lv_obj_add_flag(mqtt_notice_label, LV_OBJ_FLAG_HIDDEN);

  // AP-Modus Button
  ap_mode_btn = lv_button_create(wifi_card);
  lv_obj_set_size(ap_mode_btn, LV_PCT(100), 60);
  lv_obj_align(ap_mode_btn, LV_ALIGN_TOP_LEFT, 0, 200);
  lv_obj_set_style_bg_color(ap_mode_btn, lv_color_hex(0xFF9800), 0);
  lv_obj_set_style_bg_color(ap_mode_btn, lv_color_hex(0xF57C00), LV_STATE_PRESSED);
  lv_obj_set_style_radius(ap_mode_btn, 8, 0);
  lv_obj_add_event_cb(ap_mode_btn, on_ap_mode_clicked, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *ap_btn_label = lv_label_create(ap_mode_btn);
  lv_label_set_text(ap_btn_label, LV_SYMBOL_SETTINGS " AP-Modus aktivieren (WLAN konfigurieren)");
  lv_obj_set_style_text_font(ap_btn_label, &lv_font_montserrat_20, 0);
  lv_obj_center(ap_btn_label);

  // ========== Helligkeit Card ==========
  lv_obj_t *bright_card = lv_obj_create(container);
  lv_obj_set_size(bright_card, LV_PCT(100), 150);
  lv_obj_set_style_bg_color(bright_card, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_border_opa(bright_card, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(bright_card, 12, 0);
  lv_obj_set_style_pad_all(bright_card, 20, 0);

  // Brightness Title
  lv_obj_t *bright_title = lv_label_create(bright_card);
  lv_label_set_text(bright_title, LV_SYMBOL_IMAGE " Helligkeit");
  lv_obj_set_style_text_font(bright_title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(bright_title, lv_color_white(), 0);
  lv_obj_align(bright_title, LV_ALIGN_TOP_LEFT, 0, 0);

  // Helligkeit Label
  brightness_label = lv_label_create(bright_card);
  lv_label_set_text(brightness_label, "Helligkeit: 200");
  lv_obj_set_style_text_color(brightness_label, lv_color_hex(0xC8C8C8), 0);
  lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_20, 0);
  lv_obj_align(brightness_label, LV_ALIGN_TOP_LEFT, 0, 40);

  // Helligkeit Slider
  lv_obj_t *slider = lv_slider_create(bright_card);
  lv_obj_set_size(slider, LV_PCT(100), 20);
  lv_obj_align(slider, LV_ALIGN_TOP_LEFT, 0, 75);
  lv_slider_set_range(slider, 75, 255);
  lv_slider_set_value(slider, 200, LV_ANIM_OFF);
  lv_obj_add_event_cb(slider, on_brightness, LV_EVENT_VALUE_CHANGED, nullptr);
}

// Update WiFi Status von außen (wird von main loop aufgerufen)
void settings_update_wifi_status(bool connected, const char* ssid, const char* ip) {
  if (!wifi_status_label || !wifi_ssid_label || !wifi_ip_label) return;

  static char buf[128];

  if (connected) {
    // Verbunden
    lv_label_set_text(wifi_status_label, "Status: " LV_SYMBOL_OK " Verbunden");
    lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0x51CF66), 0);

    snprintf(buf, sizeof(buf), "Netzwerk: %s", ssid ? ssid : "---");
    lv_label_set_text(wifi_ssid_label, buf);

    snprintf(buf, sizeof(buf), "IP-Adresse: %s", ip ? ip : "---");
    lv_label_set_text(wifi_ip_label, buf);

  } else {
    // Nicht verbunden
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
