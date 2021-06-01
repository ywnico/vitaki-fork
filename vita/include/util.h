#pragma once

#include <inttypes.h>
#include <sys/types.h>

void parse_hex(const char* val, uint8_t* dest, size_t len);