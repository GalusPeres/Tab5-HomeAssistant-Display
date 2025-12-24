#include "image_popup.h"
#include "sensor_popup.h"
#include "light_popup.h"
#include <SD.h>
#include <M5Unified.h>

// Globaler Context für das Image Popup
static lv_obj_t* g_image_popup_overlay = nullptr;
static lv_obj_t* g_image_popup_img = nullptr;
static bool g_image_shown = false;
static String g_current_image_path;  // Pfad muss persistent sein für LVGL!

static void close_image_popup(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  if (g_image_popup_overlay) {
    lv_obj_del(g_image_popup_overlay);
    g_image_popup_overlay = nullptr;
    g_image_popup_img = nullptr;
    g_image_shown = false;

    // LVGL Screen neu zeichnen um das M5.Display Bild zu überschreiben
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
  }
}

void show_image_popup(const char* path) {
  if (!path || strlen(path) == 0) return;
  if (g_image_popup_overlay) return; // Schon offen

  // Andere Popups verstecken (wie light_popup und sensor_popup)
  hide_sensor_popup();
  hide_light_popup();

  // Pfad sicherstellen (mit / am Anfang)
  String fullPath = path;
  if (!fullPath.startsWith("/")) fullPath = "/" + fullPath;

  // Check SD Card file
  if (!SD.exists(fullPath)) {
    Serial.printf("[ImagePopup] Datei nicht gefunden: %s\n", fullPath.c_str());

    // Zeige Fehlermeldung als Overlay
    g_image_popup_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(g_image_popup_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(g_image_popup_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_image_popup_overlay, LV_OPA_90, 0);
    lv_obj_set_style_border_width(g_image_popup_overlay, 0, 0);
    lv_obj_add_event_cb(g_image_popup_overlay, close_image_popup, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* lbl = lv_label_create(g_image_popup_overlay);
    lv_label_set_text_fmt(lbl, "Bild nicht gefunden:\n%s", fullPath.c_str());
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);
    return;
  }

  Serial.printf("[ImagePopup] Zeige Bild: %s\n", fullPath.c_str());

  // Schwarzer Hintergrund
  g_image_popup_overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(g_image_popup_overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(g_image_popup_overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(g_image_popup_overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_image_popup_overlay, 0, 0);
  lv_obj_set_style_pad_all(g_image_popup_overlay, 0, 0);
  lv_obj_add_event_cb(g_image_popup_overlay, close_image_popup, LV_EVENT_CLICKED, nullptr);

  // LVGL Image Widget
  g_image_popup_img = lv_img_create(g_image_popup_overlay);
  g_current_image_path = "S:" + fullPath;
  lv_img_set_src(g_image_popup_img, g_current_image_path.c_str());

  // Resize Image auf Vollbild (1280x720)
  lv_obj_set_size(g_image_popup_img, 1280, 720);
  lv_obj_center(g_image_popup_img);

  g_image_shown = true;
}

void hide_image_popup() {
  if (g_image_popup_overlay) {
    lv_obj_del(g_image_popup_overlay);
    g_image_popup_overlay = nullptr;
    g_image_popup_img = nullptr;
    g_image_shown = false;

    // LVGL Screen neu zeichnen um das M5.Display Bild zu überschreiben
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
  }
}
