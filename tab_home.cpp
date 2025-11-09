// tab_home.cpp
#include "tab_home.h"
#include <lvgl.h>
#include <Arduino.h>  // for dtostrf on ESP32/Arduino
#include <cstdio>     // for snprintf
#include <cmath>      // for roundf

/* === Tuning-Konstanten === */
static const int CARD_H  = 200;   // Kachelhöhe
static const int VAL_Y   = 24;    // +Y-Offset für Wert+Einheit
static const int BTN_H   = 100;    // Button-Höhe
static const int GAP     = 24;    // Spalten-/Zeilengap
static const int OUTER   = 24;    // Außenränder im Tab

/* === Fonts === */
#if defined(LV_FONT_MONTSERRAT_18) && LV_FONT_MONTSERRAT_18
  #define FONT_TITLE (&lv_font_montserrat_18)   // Kachel-Titel
#elif defined(LV_FONT_MONTSERRAT_20) && LV_FONT_MONTSERRAT_20
  #define FONT_TITLE (&lv_font_montserrat_20)
#else
  #define FONT_TITLE (LV_FONT_DEFAULT)
#endif

#if defined(LV_FONT_MONTSERRAT_22) && LV_FONT_MONTSERRAT_22
  #define FONT_UNIT  (&lv_font_montserrat_22)   // Einheit etwas größer
#elif defined(LV_FONT_MONTSERRAT_20) && LV_FONT_MONTSERRAT_20
  #define FONT_UNIT  (&lv_font_montserrat_20)
#else
  #define FONT_UNIT  (FONT_TITLE)
#endif

#if defined(LV_FONT_MONTSERRAT_48) && LV_FONT_MONTSERRAT_48
  #define FONT_VALUE (&lv_font_montserrat_48)   // Wert groß
#elif defined(LV_FONT_MONTSERRAT_40) && LV_FONT_MONTSERRAT_40
  #define FONT_VALUE (&lv_font_montserrat_40)
#elif defined(LV_FONT_MONTSERRAT_32) && LV_FONT_MONTSERRAT_32
  #define FONT_VALUE (&lv_font_montserrat_32)
#else
  #define FONT_VALUE (LV_FONT_DEFAULT)
#endif

static inline void set_label_style(lv_obj_t* lbl, lv_color_t c, const lv_font_t* f) {
  lv_obj_set_style_text_color(lbl, c, 0);
  lv_obj_set_style_text_font(lbl,  f, 0);
}

/* === Kachel === */
// Value labels captured for runtime updates
static lv_obj_t* g_lbl_out = nullptr;
static lv_obj_t* g_lbl_in  = nullptr;
static lv_obj_t* g_lbl_soc = nullptr;
static lv_obj_t* g_lbl_wohn = nullptr; // HA: Wohnbereich Temperatur
static lv_obj_t* g_lbl_pv   = nullptr; // HA: PV Haus/Garage (W)
static scene_publish_cb_t g_scene_cb = nullptr;

static void scene_button_event_cb(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  const char* payload = static_cast<const char*>(lv_event_get_user_data(e));
  if (payload && g_scene_cb) {
    g_scene_cb(payload);
  }
}

static lv_obj_t* make_card(lv_obj_t* parent, int col, int row,
                           const char* title, const char* value, const char* unit) {
  lv_obj_t* card = lv_obj_create(parent);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x2A2A2A), 0); // helleres Grau (wie Tabbar)
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 24, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_set_style_pad_hor(card, 16, 0);
  lv_obj_set_style_pad_ver(card, 16, 0);
  lv_obj_set_height(card, CARD_H);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE); // Kachel selbst nicht scrollen

  lv_obj_set_grid_cell(card,
      LV_GRID_ALIGN_STRETCH, col, 1,
      LV_GRID_ALIGN_STRETCH, row, 1);

  // Titel
  lv_obj_t* t = lv_label_create(card);
  set_label_style(t, lv_color_hex(0xC8C8C8), FONT_TITLE);
  lv_label_set_text(t, title);
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

  // Werte-Container zentriert + etwas tiefer
  lv_obj_t* valrow = lv_obj_create(card);
  lv_obj_set_style_bg_opa(valrow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(valrow, 0, 0);
  lv_obj_set_style_pad_all(valrow, 0, 0);
  lv_obj_set_flex_flow(valrow, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(valrow, 10, 0);
  lv_obj_align(valrow, LV_ALIGN_CENTER, 0, VAL_Y);
  lv_obj_remove_flag(valrow, LV_OBJ_FLAG_SCROLLABLE);

  // Wert
  lv_obj_t* v = lv_label_create(valrow);
  set_label_style(v, lv_color_white(), FONT_VALUE);
  lv_label_set_text(v, value);

  // Einheit
  if (unit && unit[0]) {
    lv_obj_t* u = lv_label_create(valrow);
    set_label_style(u, lv_color_hex(0xE0E0E0), FONT_UNIT);
    lv_label_set_text(u, unit);
  }

  // Capture first three value labels for Home tab updates
  if (!g_lbl_out)      g_lbl_out = v;
  else if (!g_lbl_in)  g_lbl_in  = v;
  else if (!g_lbl_soc) g_lbl_soc = v;

  // Return value label so caller can update it later
  return v;
}

/* === Szenen-Button === */
static lv_obj_t* make_scene_button(lv_obj_t* parent, int col, int row,
                                   const char* text, const char* payload = nullptr) {
  lv_obj_t* btn = lv_button_create(parent);

  // Grundstil
  lv_obj_set_style_radius(btn, 16, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_height(btn, BTN_H);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

  // **Keine** Schatten/Outlines, keine Transparenz-Säume
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_outline_width(btn, 0, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_FOCUSED);

  // Durchgehend dunkle Farben in allen States
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A2A2A), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x3A3A3A), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A2A2A), LV_PART_MAIN | LV_STATE_FOCUSED);

  lv_obj_set_grid_cell(btn,
      LV_GRID_ALIGN_STRETCH, col, 1,
      LV_GRID_ALIGN_STRETCH, row, 1);

  lv_obj_t* l = lv_label_create(btn);
  set_label_style(l, lv_color_white(), FONT_TITLE);
  lv_label_set_text(l, text);
  lv_obj_center(l);

  const char* user_payload = payload ? payload : text;
  lv_obj_add_event_cb(btn, scene_button_event_cb, LV_EVENT_CLICKED, (void*)user_payload);

  return btn;
}

void build_home_tab(lv_obj_t *parent, scene_publish_cb_t scene_cb) {
  g_scene_cb = scene_cb;
  // Nur der Tab (parent) darf scrollen
  lv_obj_set_style_bg_color(parent, lv_color_hex(0x111111), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  lv_obj_set_scroll_dir(parent, LV_DIR_VER);
  lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_anim_duration(parent, 0, 0);

  // Symmetrische Außenränder
  lv_obj_set_style_pad_left(parent,   OUTER, 0);
  lv_obj_set_style_pad_right(parent,  OUTER, 0);
  lv_obj_set_style_pad_top(parent,    OUTER, 0);
  lv_obj_set_style_pad_bottom(parent, OUTER, 0);

  // Grid: 3 Spalten, 3 Zeilen (Mitte = flexibler Spacer, Buttons unten kleben)
  lv_obj_t* grid = lv_obj_create(parent);

  // ***Fix gegen „weißes Säumen“ unten: deckender Grid-Hintergrund***
  lv_obj_set_style_bg_color(grid, lv_color_hex(0x111111), 0);
  lv_obj_set_style_bg_opa(grid, LV_OPA_COVER, 0);

  lv_obj_set_style_border_width(grid, 0, 0);
  lv_obj_set_style_pad_all(grid, 0, 0);
  lv_obj_set_size(grid, lv_pct(100), lv_pct(100));
  lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

  static lv_coord_t col_dsc[] = {
    LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST
  };
  static lv_coord_t row_dsc[] = {
    LV_GRID_CONTENT,  // 0: Karten
    LV_GRID_CONTENT,  // 1: weitere Karten (HA)
    LV_GRID_CONTENT,  // 2: Buttons unten
    LV_GRID_TEMPLATE_LAST
  };
  lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

  lv_obj_set_style_pad_column(grid, GAP, 0);
  lv_obj_set_style_pad_row(grid, GAP, 0);

  // Karten (Zeile 0)
  (void)make_card(grid, 0, 0, "Außentemperatur", "21.7", "°C");
  (void)make_card(grid, 1, 0, "Innentemperatur", "22.4", "°C");
  (void)make_card(grid, 2, 0, "Batterie (SoC)",  "73",   "%");

  // Zusätzliche Karten (Zeile 1) aus Home Assistant
  g_lbl_wohn = make_card(grid, 0, 1, "Wohnbereich", "--", "°C");
  g_lbl_pv   = make_card(grid, 1, 1, "PV Garage",  "--", "W");

  // Buttons (Zeile 2) -> kleben am unteren Rand
  (void)make_scene_button(grid, 0, 2, "Aus",   "aus");
  (void)make_scene_button(grid, 1, 2, "Lesen", "lesen");
  (void)make_scene_button(grid, 2, 2, "PC",    "pc");
}

// Public API to update values from other modules (e.g., MQTT)
void home_set_values(float outside_c, float inside_c, int soc_pct) {
  if (g_lbl_out) {
    char b1[16];
    dtostrf(outside_c, 0, 1, b1);
    lv_label_set_text(g_lbl_out, b1);
  }
  if (g_lbl_in) {
    char b2[16];
    dtostrf(inside_c, 0, 1, b2);
    lv_label_set_text(g_lbl_in, b2);
  }
  if (g_lbl_soc) {
    char b3[8];
    snprintf(b3, sizeof(b3), "%d", soc_pct);
    lv_label_set_text(g_lbl_soc, b3);
  }
}

void home_set_wohn_temp(float celsius) {
  if (g_lbl_wohn) {
    char buf[16];
    dtostrf(celsius, 0, 1, buf);
    lv_label_set_text(g_lbl_wohn, buf);
  }
}

void home_set_pv_garage(float watts) {
  if (g_lbl_pv) {
    char buf[16];
    // typische PV-Leistung als ganze Watt anzeigen
    snprintf(buf, sizeof(buf), "%d", (int)roundf(watts));
    lv_label_set_text(g_lbl_pv, buf);
  }
}
