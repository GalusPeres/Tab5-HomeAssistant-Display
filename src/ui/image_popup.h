#ifndef IMAGE_POPUP_H
#define IMAGE_POPUP_H

#include <lvgl.h>
#include <Arduino.h>

// Zeigt ein Bild von der SD-Karte als Fullscreen-Popup an
// path: Pfad zur Datei (z.B. "/bild.jpg")
void show_image_popup(const char* path);
void hide_image_popup();

#endif // IMAGE_POPUP_H
