#pragma once
#include <chiaki/common.h>
#include <chiaki/controller.h>

// TODO controls should be made fully customizable in the future.
// For now, use a set of defined maps
typedef enum vitaki_controller_map_id_t {
  VITAKI_CONTROLLER_MAP_0      =  0, // L2, R2 rear touchscreen upper corners; L3, R3 rear touchscreen lower corners; touchpad entire front
  VITAKI_CONTROLLER_MAP_1      =  1, // L2, R2 front touchscreen upper corners; L3, R3 front touchscreen lower corners; touchpad front middle
  VITAKI_CONTROLLER_MAP_2      =  2, // L2, R2 front touchscreen lower corners; L3 R3 on back; touchpad front middle
  VITAKI_CONTROLLER_MAP_3      =  3, // L2, R2 front touchscreen upper corners; L3 R3 on back; touchpad front middle
  VITAKI_CONTROLLER_MAP_4      =  4, // no L2,R2,L3,R3 mapping; touchpad entire front
  VITAKI_CONTROLLER_MAP_5      =  5, // no L2,R2,L3,R3,touchpad mapping
  VITAKI_CONTROLLER_MAP_6      =  6, // L2, R2 front touchscreen lower corners; no L3 R3; touchpad front middle
  VITAKI_CONTROLLER_MAP_7      =  7, // L2, R2 front touchscreen upper corners; no L3 R3; touchpad front middle
  VITAKI_CONTROLLER_MAP_25     = 25, // L2, R2 front touchscreen upper corners; L3, R3 front touchscreen lower corners; no touchpad
  VITAKI_CONTROLLER_MAP_YWNICO = 99, // map ywnico randomly came up with
} VitakiControllerMapId;

// Control buttons (used for array indices => start at 0)
typedef enum vitaki_ctrl_in_t {
  VITAKI_CTRL_IN_NONE = 0,

  VITAKI_CTRL_IN_L1,
  VITAKI_CTRL_IN_R1,

  VITAKI_CTRL_IN_SELECT_START,
  VITAKI_CTRL_IN_LEFT_SQUARE,
  VITAKI_CTRL_IN_RIGHT_CIRCLE,

  VITAKI_CTRL_IN_REARTOUCH_UL,
  VITAKI_CTRL_IN_REARTOUCH_UR,
  VITAKI_CTRL_IN_REARTOUCH_LL,
  VITAKI_CTRL_IN_REARTOUCH_LR,
  VITAKI_CTRL_IN_REARTOUCH_LEFT,
  VITAKI_CTRL_IN_REARTOUCH_RIGHT,
  VITAKI_CTRL_IN_REARTOUCH_ANY,
  VITAKI_CTRL_IN_REARTOUCH_LEFT_L1,
  VITAKI_CTRL_IN_REARTOUCH_RIGHT_R1,

  VITAKI_CTRL_IN_FRONTTOUCH_UL_ARC,
  VITAKI_CTRL_IN_FRONTTOUCH_UR_ARC,
  VITAKI_CTRL_IN_FRONTTOUCH_LL_ARC,
  VITAKI_CTRL_IN_FRONTTOUCH_LR_ARC,
  VITAKI_CTRL_IN_FRONTTOUCH_LEFT,
  VITAKI_CTRL_IN_FRONTTOUCH_RIGHT,
  VITAKI_CTRL_IN_FRONTTOUCH_CENTER,
  VITAKI_CTRL_IN_FRONTTOUCH_ANY,

  VITAKI_CTRL_IN_COUNT, // final element to count the total number
} VitakiCtrlIn;

typedef enum vitaki_ctrl_out_t {
  VITAKI_CTRL_OUT_UP       = CHIAKI_CONTROLLER_BUTTON_DPAD_UP,
  VITAKI_CTRL_OUT_DOWN     = CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN,
  VITAKI_CTRL_OUT_LEFT     = CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT,
  VITAKI_CTRL_OUT_RIGHT    = CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT,

  VITAKI_CTRL_OUT_TRIANGLE = CHIAKI_CONTROLLER_BUTTON_PYRAMID,
  VITAKI_CTRL_OUT_CIRCLE   = CHIAKI_CONTROLLER_BUTTON_MOON,
  VITAKI_CTRL_OUT_CROSS    = CHIAKI_CONTROLLER_BUTTON_CROSS,
  VITAKI_CTRL_OUT_SQUARE   = CHIAKI_CONTROLLER_BUTTON_BOX,

  VITAKI_CTRL_OUT_SHARE    = CHIAKI_CONTROLLER_BUTTON_SHARE,
  VITAKI_CTRL_OUT_OPTIONS  = CHIAKI_CONTROLLER_BUTTON_OPTIONS,
  VITAKI_CTRL_OUT_PS       = CHIAKI_CONTROLLER_BUTTON_PS,
  VITAKI_CTRL_OUT_TOUCHPAD = CHIAKI_CONTROLLER_BUTTON_TOUCHPAD,

  VITAKI_CTRL_OUT_L1       = CHIAKI_CONTROLLER_BUTTON_L1,
  VITAKI_CTRL_OUT_R1       = CHIAKI_CONTROLLER_BUTTON_R1,
  VITAKI_CTRL_OUT_L3       = CHIAKI_CONTROLLER_BUTTON_L3,
  VITAKI_CTRL_OUT_R3       = CHIAKI_CONTROLLER_BUTTON_R3,

  // TODO be careful about avoiding collision
  VITAKI_CTRL_OUT_L2       = 3,
  VITAKI_CTRL_OUT_R2       = 7,

  // VITAKI_CTRL_OUT_NONE must be 0 for logic to work
  VITAKI_CTRL_OUT_NONE     = 0,
} VitakiCtrlOut;

typedef struct vitaki_ctrl_map_info_t {
  bool in_state[VITAKI_CTRL_IN_COUNT];
  int in_out_btn[VITAKI_CTRL_IN_COUNT];
  int in_l2;
  int in_r2;
  bool did_init;
} VitakiCtrlMapInfo;

void init_controller_map(VitakiCtrlMapInfo* vcmi, VitakiControllerMapId controller_map_id);
