#include "lvgl_psram_alloc.h"

#include "esp_heap_caps.h"

extern "C" void* lvgl_mem_pool_alloc(size_t size) {
  void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!ptr) {
    ptr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }
  return ptr;
}
