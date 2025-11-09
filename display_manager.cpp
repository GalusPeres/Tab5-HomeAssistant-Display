#include "display_manager.h"
#include "power_manager.h"
#include <M5Unified.h>
#include "esp_heap_caps.h"
#include <Arduino.h>

// Globale Instanz
DisplayManager displayManager;

// Statische Member-Variablen
lv_display_t* DisplayManager::disp = nullptr;
lv_indev_t* DisplayManager::indev = nullptr;
lv_color_t* DisplayManager::buf1 = nullptr;
lv_color_t* DisplayManager::buf2 = nullptr;
uint32_t DisplayManager::last_activity_time = 0;

// ========== Display Flush Callback ==========
void DisplayManager::flush_cb(lv_display_t *lv_disp, const lv_area_t *area, uint8_t *px_map) {
  const uint32_t w = (area->x2 - area->x1 + 1);
  const uint32_t h = (area->y2 - area->y1 + 1);
  M5.Display.pushImageDMA(area->x1, area->y1, w, h, (uint16_t*)px_map);
  lv_display_flush_ready(lv_disp);
}

// ========== Touch Callback ==========
void DisplayManager::touch_cb(lv_indev_t* indev_drv, lv_indev_data_t *data) {
  lgfx::touch_point_t tp;
  if (M5.Display.getTouch(&tp)) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = tp.x;
    data->point.y = tp.y;

    // Activity-Timer zurücksetzen und Power Manager aufwecken
    last_activity_time = millis();
    powerManager.setHighPerformance(true);
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// ========== Initialisierung ==========
bool DisplayManager::init() {
  Serial.println("🖥️ Initialisiere Display Manager...");

  // M5Stack Display-Setup
  M5.Display.setRotation(1);  // 1280x720 Querformat
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(150);  // Wird später vom Power Manager gesteuert

  last_activity_time = millis();

  // LVGL initialisieren
  lv_init();

  // Display erstellen
  disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  if (!disp) {
    Serial.println("❌ Display-Erstellung fehlgeschlagen!");
    return false;
  }

  lv_display_set_flush_cb(disp, flush_cb);

  // Farbformat + Anti-Aliasing aus (Performance)
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
  lv_display_set_antialiasing(disp, false);

  // Große DMA-Puffer (200 Zeilen) → weniger Flushes pro Frame
  const size_t LINES = 200;
  const size_t buf_bytes = SCREEN_WIDTH * LINES * sizeof(lv_color_t);

  buf1 = (lv_color_t*)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
  buf2 = (lv_color_t*)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);

  if (!buf1 || !buf2) {
    Serial.println("❌ DMA-Buffer-Allokation fehlgeschlagen!");
    return false;
  }

  lv_display_set_buffers(disp, buf1, buf2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  Serial.printf("✓ DMA-Puffer: 2x %d Bytes (je %d Zeilen)\n", buf_bytes, LINES);

  // Touch-Input
  indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_cb);
  lv_indev_set_display(indev, disp);

  Serial.println("✓ Display Manager initialisiert");
  return true;
}

void DisplayManager::resetActivityTimer() {
  last_activity_time = millis();
}
