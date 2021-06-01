#include <stdio.h>
#include "util.h"

void parse_hex(const char* val, uint8_t* dest, size_t len) {
  for (size_t i=0; i < len; i++) {
    sscanf(val+(i*2), "%02hhx", &(dest[i]));
  }
}
