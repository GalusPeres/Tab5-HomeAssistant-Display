#include "power_manager.h"
#include <M5Unified.h>
#include "display_manager.h"

// Globale Instanz
PowerManager powerManager;

// ========== Initialisierung ==========
void PowerManager::init() {
  Serial.println("⚡ Initialisiere Power Manager...");

  // CPU-Frequenz auf Maximum für schnelle Initialisierung
  setCpuFrequencyMhz(CPU_FREQ_HIGH);
  Serial.printf("✓ CPU Freq: %d MHz\n", getCpuFrequencyMhz());

  // Display-Referenz holen
  disp = displayManager.getDisplay();

  is_high_performance = true;

  Serial.printf("⚡ High-Performance: %d MHz, %d FPS\n",
                CPU_FREQ_HIGH, FPS_HIGH);
  Serial.printf("💤 Power-Saving: %d MHz, %d FPS\n",
                CPU_FREQ_LOW, FPS_LOW);
}

// ========== Performance-Modus setzen ==========
void PowerManager::setHighPerformance(bool enable) {
  if (enable == is_high_performance) return;

  is_high_performance = enable;

  if (enable) {
    // ULTRA-SCHNELL bei Touch!
    setCpuFrequencyMhz(CPU_FREQ_HIGH);

#if LVGL_VERSION_MAJOR >= 9
    if (disp && lv_display_get_refr_timer) {
      lv_timer_t *rt = lv_display_get_refr_timer(disp);
      if (rt) lv_timer_set_period(rt, 1000 / FPS_HIGH);  // 60 FPS
    }
#endif
    Serial.println("⚡ HIGH PERFORMANCE MODE (CPU + FPS)");

  } else {
    // Stromsparen bei Inaktivität (nur CPU + FPS, Helligkeit bleibt unverändert)
    setCpuFrequencyMhz(CPU_FREQ_LOW);

#if LVGL_VERSION_MAJOR >= 9
    if (disp && lv_display_get_refr_timer) {
      lv_timer_t *rt = lv_display_get_refr_timer(disp);
      if (rt) lv_timer_set_period(rt, 1000 / FPS_LOW);  // 10 FPS
    }
#endif
    Serial.println("💤 POWER SAVING MODE (CPU + FPS)");
  }
}

// ========== Update (Idle-Detection) ==========
void PowerManager::update(uint32_t last_activity_time) {
  uint32_t now = millis();

  // Prüfen ob Idle-Timeout erreicht
  if (is_high_performance && (now - last_activity_time > IDLE_TIMEOUT_MS)) {
    setHighPerformance(false);  // Stromsparen aktivieren
  }
}
