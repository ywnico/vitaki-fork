#include <stdio.h>
#include "util.h"

void parse_hex(const char* val, uint8_t* dest, size_t len) {
  for (size_t i=0; i < len; i++) {
    sscanf(val+(i*2), "%02hhx", &(dest[i]));
  }
}

/*
  VitaShell
  Copyright (C) 2015-2018, TheFloW
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
void utf16_to_utf8(const uint16_t *src, uint8_t *dst) {
  int i;
  for (i = 0; src[i]; i++) {
    if ((src[i] & 0xFF80) == 0) {
      *(dst++) = src[i] & 0xFF;
    } else if((src[i] & 0xF800) == 0) {
      *(dst++) = ((src[i] >> 6) & 0xFF) | 0xC0;
      *(dst++) = (src[i] & 0x3F) | 0x80;
    } else if((src[i] & 0xFC00) == 0xD800 && (src[i + 1] & 0xFC00) == 0xDC00) {
      *(dst++) = (((src[i] + 64) >> 8) & 0x3) | 0xF0;
      *(dst++) = (((src[i] >> 2) + 16) & 0x3F) | 0x80;
      *(dst++) = ((src[i] >> 4) & 0x30) | 0x80 | ((src[i + 1] << 2) & 0xF);
      *(dst++) = (src[i + 1] & 0x3F) | 0x80;
      i += 1;
    } else {
      *(dst++) = ((src[i] >> 12) & 0xF) | 0xE0;
      *(dst++) = ((src[i] >> 6) & 0x3F) | 0x80;
      *(dst++) = (src[i] & 0x3F) | 0x80;
    }
  }

  *dst = '\0';
}

void utf8_to_utf16(const uint8_t *src, uint16_t *dst) {
  int i;
  for (i = 0; src[i];) {
    if ((src[i] & 0xE0) == 0xE0) {
      *(dst++) = ((src[i] & 0x0F) << 12) | ((src[i + 1] & 0x3F) << 6) | (src[i + 2] & 0x3F);
      i += 3;
    } else if ((src[i] & 0xC0) == 0xC0) {
      *(dst++) = ((src[i] & 0x1F) << 6) | (src[i + 1] & 0x3F);
      i += 2;
    } else {
      *(dst++) = src[i];
      i += 1;
    }
  }

  *dst = '\0';
}

size_t get_base64_size(size_t in)
{
	// calculate base64 buffer size after encode
	return ((4 * in / 3) + 3) & ~3;
}