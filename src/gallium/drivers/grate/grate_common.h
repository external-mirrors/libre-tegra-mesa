#ifndef GRATE_COMMON_H
#define GRATE_COMMON_H

#include "grate_screen.h"

#define grate_unimplemented() do { \
   if (grate_debug & GRATE_DEBUG_UNIMPLEMENTED) \
      printf("GRATE TODO: %s()\n", __func__); \
} while (0)

#define grate_trace() do { \
   if (grate_debug & GRATE_DEBUG_TRACE) \
      printf("GRATE: %s()\n", __func__); \
} while (0)

#endif
