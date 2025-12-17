# Statusbericht (16.12.2025) – Scene-Buttons, Icons, Settings-Freeze

## Kontext
- Ziel: MDI-Icons (48 px) nutzen; Mapping erfolgt über einen MDI-Extractor, damit im Webinterface nur Symbolnamen (z.B. `home`) eingegeben werden.
- Hardware: ESP32-P4 (sollte 48er Icons packen).
- Branches/Stände:
  - `scene-buttons-working-no-icons` (74cb592, 16.12. 13:51): Szenen-Buttons funktionieren; keine MDI-Icons eingebunden; Sensor-Tiles minimal (ein Label für Wert+Einheit); Settings reagiert.
  - `restore-5ed2560`: Neuerer Stand mit 48er Icons, Value/Unit getrennt (Flex-Layout), zusätzliche Layout-Offsets; hier traten Probleme auf.
  - Stash: `temp-before-scene-buttons-checkout` (sichert den Zustand vor dem Wechsel zurück auf 74cb592).

## Beobachtete Probleme
- Scene-Buttons (vor allem mit Icon) reagierten manchmal nicht. In späteren Ständen wurden Event-/State-Resets ausprobiert; stabil war der ältere Snapshot ohne Icon-Font und mit minimalem Tile-Layout.
- Touch auf Settings-Tab verursachte Freeze (keine Logs mehr). Auskommentieren der Icons half nicht. Vermutung: einmalige Render-Last (viele Widgets + große Fonts/Icons).

## Änderungen/Experimente
1) Icons verkleinert (48 → 32) und später ganz deaktiviert. Ergebnis: Freeze/Tap-Problem blieb bestehen.
2) Settings-Warm-up eingebaut: Tab kurz sichtbar, mehrere `lv_timer_handler()` + kleine Delays, dann wieder versteckt – soll das erste Öffnen entlasten.
3) Sensor-Tile-Layout geändert (in neueren Ständen): Wert und Einheit als zwei Labels in einem Flex-Container, Unit kleiner, zusätzliche Offsets. Im funktionierenden Snapshot: nur ein Label mit kombiniertem Text für Wert+Einheit, kein Flex, kein Unit-Label.
4) Mehrere Touch-/State-Resets für Scene-Buttons getestet (verschiedene Commit-Stände), später verworfen; stabiler Zustand blieb der alte Snapshot.

## Hypothesen
- Einstellungen-Freeze: Hohe Render-Last beim ersten Unhide des Settings-Tabs (viele Widgets). Große Fonts/Icons verstärken das. Warm-up hilft, ersetzt aber nicht schlankere Struktur.
- Scene-Buttons: Zusätzliche Event-/State-Manipulationen oder Layout-Komplexität (z.B. Flex mit zwei Labels) könnten Timing/Touch-Probleme verstärken. Im einfachen Layout ohne Icon-Font lief es.
- Icons alleine sind nicht die alleinige Ursache, weil das Problem auch ohne Icons auftrat, sobald das Layout komplexer war.

## Empfohlene Vorgehensweise (für nächste KI)
1) Basis beibehalten wie im funktionierenden Snapshot:
   - Sensor-Tiles: EIN Label für Wert+Einheit (kombinierter Text, zentriert, kein Flex, keine zweite Unit-Komponente).
   - Scene-Buttons: Einfacher `LV_EVENT_CLICKED`-Handler, keine nachträglichen State-Clears/Resets, falls nicht zwingend nötig.
2) Icons (48 px) wieder hinzufügen:
   - MDI-Font `mdi_icons_48` einbinden.
   - Im Sensor-Titel optional ein Icon-Label vor den Text setzen; einfache Positionierung (z.B. `lv_obj_align_to`), kein Flex-Zwang.
   - Mapping über den bestehenden MDI-Extractor nutzen (Namen wie `home`, `thermometer` etc.).
3) Settings-Tab entlasten:
   - Warm-up-Render beibehalten (mehrere `lv_timer_handler()` + kleine Delays nach dem ersten Unhide, dann wieder hide).
   - Falls weiter Hänger: Unterseiten lazy aufbauen (erst Main Menu, Unterseiten bei Bedarf anlegen oder erst bei erstem Öffnen rendern).
4) Performance/RAM:
   - 48 px, 4bpp + Font-Compression ist ok für ESP32-P4; trotzdem nur benötigte Glyph-Ranges extrahieren.
   - Flex nur nutzen, wenn nötig; statische Aligns sind schneller/planbarer.
5) Testschritte nach jedem Umbau:
   - Direkt nach Boot: Settings antippen (prüfen, ob Freeze).
   - Scene-Buttons mehrfach tippen (auch solche mit Icons), prüfen, ob Events feuern.

## Was sich zwischen „geht“ und „geht nicht“ unterscheidet
- Sensor-Tiles: Minimal (ein Label, kein Flex) vs. zwei Labels in Flex-Row mit Offsets.
- Icons: Komplett aus vs. großer MDI-Font eingebunden.
- Settings-Tab: Warm-up vorhanden vs. fehlend; Render-Last unverändert hoch.

## Persönliche Erfahrung (Assistant)
- Häufige Ursache für UI-Hänger in LVGL ist das erste Zeichnen großer, komplexer Container. Vor-Rendern (Warm-up) verhindert, dass der erste User-Tap blockiert.
- Layout-Komplexität (Flex + viele Labels) kann Touch-Verhalten indirekt beeinflussen, wenn die Render-Pipeline ausgelastet ist. Weniger Widgets/keine verschachtelten Flex-Container waren spürbar stabiler.
- Icons deaktivieren allein reicht nicht; die Kombination aus großem Font + Layout-Änderungen und schwerem Settings-Tab war ausschlaggebend. Minimal halten, dann Icons schrittweise hinzufügen, ist der sicherste Weg. 
