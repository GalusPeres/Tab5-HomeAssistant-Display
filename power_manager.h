#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include <lvgl.h>

// ========== Power Management Einstellungen ==========
#define CPU_FREQ_HIGH       360   // Ultra-schnell bei Touch
#define CPU_FREQ_LOW        80    // Stromsparen bei Inaktivität
#define BRIGHTNESS_HIGH     150   // Hell bei Interaktion
#define BRIGHTNESS_LOW      40    // Gedimmt bei Inaktivität
#define FPS_HIGH            60    // 60 FPS für flüssige Bedienung
#define FPS_LOW             10    // 10 FPS zum Stromsparen
#define IDLE_TIMEOUT_MS     8000  // 8 Sekunden bis Idle-Modus

// Power Manager - Verwaltet Energiemodi und Performance
class PowerManager {
public:
  // Initialisierung
  void init();

  // Performance-Modus setzen
  void setHighPerformance(bool enable);

  // Status
  bool isHighPerformance() const { return is_high_performance; }

  // Update-Schleife (prüft Idle-Timeout)
  void update(uint32_t last_activity_time);

private:
  bool is_high_performance = true;
  lv_display_t* disp = nullptr;  // Referenz zum Display (für FPS-Anpassung)
};

// Globale Instanz
extern PowerManager powerManager;

#endif // POWER_MANAGER_H
