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
} VitaChiakiUIState;

void draw_ui();