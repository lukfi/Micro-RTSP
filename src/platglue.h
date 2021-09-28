#pragma once

#ifdef ARDUINO_ARCH_ESP32
#include "platglue-esp32.h"
#elif defined(USE_LFF)
#include "platglue-lf.h"
#else
#include "platglue-posix.h"
#endif
