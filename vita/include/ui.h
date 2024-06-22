#pragma once
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <vita2d.h>

typedef struct vita_chiaki_ui_state_t {
  SceTouchData touch_state_front;
  SceTouchData touch_state_back;
  uint32_t button_state;
  uint32_t old_button_state;
  int active_item;
  int next_active_item;
  int mlog_line_offset;
  uint64_t mlog_last_update;
} VitaChiakiUIState;

void draw_ui();