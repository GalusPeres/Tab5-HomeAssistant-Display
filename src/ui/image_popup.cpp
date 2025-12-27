#include "image_popup.h"
#include "sensor_popup.h"
#include "light_popup.h"
#include <SD.h>
#include <M5Unified.h>
#if __has_include(<draw/lv_image_decoder_private.h>)
#include <draw/lv_image_decoder_private.h>
#elif __has_include("src/draw/lv_image_decoder_private.h")
#include "src/draw/lv_image_decoder_private.h"
#else
#error "lv_image_decoder_private.h not found"
#endif

// Globaler Context fuer das Image Popup
static lv_obj_t* g_image_popup_overlay = nullptr;
static lv_obj_t* g_image_popup_img = nullptr;
static lv_obj_t* g_image_popup_label = nullptr;
static bool g_image_shown = false;
static lv_image_header_t g_current_header;
static bool g_current_header_valid = false;
static String g_current_image_path;  // Pfad muss persistent sein fuer LVGL!

static void close_image_popup(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  hide_image_popup();
}

static void ensure_image_popup_overlay() {
  if (g_image_popup_overlay) return;

  g_image_popup_overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(g_image_popup_overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(g_image_popup_overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(g_image_popup_overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_image_popup_overlay, 0, 0);
  lv_obj_set_style_pad_all(g_image_popup_overlay, 0, 0);
  lv_obj_add_event_cb(g_image_popup_overlay, close_image_popup, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_flag(g_image_popup_overlay, LV_OBJ_FLAG_HIDDEN);

  g_image_popup_img = lv_img_create(g_image_popup_overlay);
  lv_image_set_antialias(g_image_popup_img, false);

  g_image_popup_label = lv_label_create(g_image_popup_overlay);
  lv_obj_set_style_text_color(g_image_popup_label, lv_color_white(), 0);
  lv_label_set_long_mode(g_image_popup_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(g_image_popup_label, LV_PCT(90));
  lv_obj_center(g_image_popup_label);
  lv_obj_add_flag(g_image_popup_label, LV_OBJ_FLAG_HIDDEN);
}

static void show_image_popup_error(const char* title, const char* path) {
  ensure_image_popup_overlay();

  lv_obj_set_style_bg_opa(g_image_popup_overlay, LV_OPA_90, 0);
  lv_obj_clear_flag(g_image_popup_overlay, LV_OBJ_FLAG_HIDDEN);

  if (g_image_popup_img) {
    lv_obj_add_flag(g_image_popup_img, LV_OBJ_FLAG_HIDDEN);
  }

  if (g_image_popup_label) {
    lv_label_set_text_fmt(g_image_popup_label, "%s:\n%s", title, path);
    lv_obj_clear_flag(g_image_popup_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(g_image_popup_label);
  }

  g_image_shown = true;
}

void show_image_popup(const char* path) {
  if (!path || strlen(path) == 0) return;

  // Andere Popups verstecken (wie light_popup und sensor_popup)
  hide_sensor_popup();
  hide_light_popup();

  // Pfad sicherstellen (mit / am Anfang)
  String fullPath = path;
  if (!fullPath.startsWith("/")) fullPath = "/" + fullPath;

  // Check SD Card file
  if (!SD.exists(fullPath)) {
    Serial.printf("[ImagePopup] Datei nicht gefunden: %s\n", fullPath.c_str());

    show_image_popup_error("Bild nicht gefunden", fullPath.c_str());
    return;
  }

  Serial.printf("[ImagePopup] Zeige Bild: %s\n", fullPath.c_str());

  String new_src = "S:" + fullPath;
  lv_image_header_t header;
  if (g_current_image_path == new_src && g_current_header_valid) {
    header = g_current_header;
  } else {
    if (lv_image_decoder_get_info(new_src.c_str(), &header) != LV_RESULT_OK) {
      Serial.printf("[ImagePopup] LVGL kann Bild nicht dekodieren: %s\n", fullPath.c_str());
      show_image_popup_error("Bildformat nicht unterstuetzt", fullPath.c_str());
      return;
    }
    g_current_header = header;
    g_current_header_valid = true;
  }

  ensure_image_popup_overlay();

  lv_obj_set_style_bg_opa(g_image_popup_overlay, LV_OPA_COVER, 0);
  lv_obj_clear_flag(g_image_popup_overlay, LV_OBJ_FLAG_HIDDEN);

  if (g_current_image_path != new_src) {
    g_current_image_path = new_src;
    lv_img_set_src(g_image_popup_img, g_current_image_path.c_str());
  }

  // Native size to avoid extra scaling work.
  lv_obj_set_size(g_image_popup_img, header.w, header.h);
  lv_obj_center(g_image_popup_img);
  lv_obj_clear_flag(g_image_popup_img, LV_OBJ_FLAG_HIDDEN);
  if (g_image_popup_label) lv_obj_add_flag(g_image_popup_label, LV_OBJ_FLAG_HIDDEN);

  g_image_shown = true;
}

void hide_image_popup() {
  if (g_image_popup_overlay) {
    lv_obj_add_flag(g_image_popup_overlay, LV_OBJ_FLAG_HIDDEN);
    g_image_shown = false;
  }
}

void preload_image_popup(const char* path) {
  if (!path || strlen(path) == 0) return;

  String fullPath = path;
  if (!fullPath.startsWith("/")) fullPath = "/" + fullPath;
  if (!SD.exists(fullPath)) return;

  String src = "S:" + fullPath;
  lv_image_decoder_dsc_t dsc;
  if (lv_image_decoder_open(&dsc, src.c_str(), nullptr) == LV_RESULT_OK) {
    lv_image_decoder_close(&dsc);
  }
}
