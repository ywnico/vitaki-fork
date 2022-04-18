#pragma once

#include <inttypes.h>
#include <sys/types.h>

void parse_hex(const char* val, uint8_t* dest, size_t len);

void utf16_to_utf8(const uint16_t *src, uint8_t *dst);

void utf8_to_utf16(const uint8_t *src, uint16_t *dst);

size_t get_base64_size(size_t in);