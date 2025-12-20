#pragma once

#if !defined(__ASSEMBLER__) && !defined(__ASSEMBLY__)
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* lvgl_mem_pool_alloc(size_t size);

#ifdef __cplusplus
}
#endif
#endif
