#include "controller.h"
#include "context.h"
#include <stdio.h>

void init_controller_map(VitakiCtrlMapInfo* vcmi) {

  // init controller map
  // TODO make configurable
  memset(vcmi->in_out_btn, 0, sizeof(vcmi->in_out_btn));
  vcmi->in_out_btn[VITAKI_CTRL_IN_L1]                  = VITAKI_CTRL_OUT_L1;
  vcmi->in_out_btn[VITAKI_CTRL_IN_R1]                  = VITAKI_CTRL_OUT_R1;
  vcmi->in_out_btn[VITAKI_CTRL_IN_SELECT_START]        = VITAKI_CTRL_OUT_PS;
  vcmi->in_out_btn[VITAKI_CTRL_IN_LEFT_SQUARE]         = VITAKI_CTRL_OUT_L3;
  vcmi->in_out_btn[VITAKI_CTRL_IN_RIGHT_CIRCLE]        = VITAKI_CTRL_OUT_R3;
  vcmi->in_out_btn[VITAKI_CTRL_IN_REARTOUCH_UL]        = VITAKI_CTRL_OUT_NONE;
  vcmi->in_out_btn[VITAKI_CTRL_IN_REARTOUCH_UR]        = VITAKI_CTRL_OUT_NONE;
  vcmi->in_out_btn[VITAKI_CTRL_IN_REARTOUCH_LL]        = VITAKI_CTRL_OUT_NONE;
  vcmi->in_out_btn[VITAKI_CTRL_IN_REARTOUCH_LR]        = VITAKI_CTRL_OUT_NONE;
  vcmi->in_out_btn[VITAKI_CTRL_IN_REARTOUCH_LEFT]      = VITAKI_CTRL_OUT_NONE;
  vcmi->in_out_btn[VITAKI_CTRL_IN_REARTOUCH_RIGHT]     = VITAKI_CTRL_OUT_NONE;
  vcmi->in_out_btn[VITAKI_CTRL_IN_REARTOUCH_ANY]       = VITAKI_CTRL_OUT_NONE;
  vcmi->in_out_btn[VITAKI_CTRL_IN_REARTOUCH_LEFT_L1]   = VITAKI_CTRL_OUT_NONE;
  vcmi->in_out_btn[VITAKI_CTRL_IN_REARTOUCH_RIGHT_R1]  = VITAKI_CTRL_OUT_NONE;
  vcmi->in_out_btn[VITAKI_CTRL_IN_FRONTTOUCH_UL]       = VITAKI_CTRL_OUT_NONE;
  vcmi->in_out_btn[VITAKI_CTRL_IN_FRONTTOUCH_UR]       = VITAKI_CTRL_OUT_NONE;
  vcmi->in_out_btn[VITAKI_CTRL_IN_FRONTTOUCH_LL]       = VITAKI_CTRL_OUT_NONE;
  vcmi->in_out_btn[VITAKI_CTRL_IN_FRONTTOUCH_LR]       = VITAKI_CTRL_OUT_NONE;
  vcmi->in_out_btn[VITAKI_CTRL_IN_FRONTTOUCH_LEFT]     = VITAKI_CTRL_OUT_NONE;
  vcmi->in_out_btn[VITAKI_CTRL_IN_FRONTTOUCH_RIGHT]    = VITAKI_CTRL_OUT_NONE;
  vcmi->in_out_btn[VITAKI_CTRL_IN_FRONTTOUCH_ANY]      = VITAKI_CTRL_OUT_TOUCHPAD;


  vcmi->in_l2 = VITAKI_CTRL_IN_REARTOUCH_LEFT_L1;
  vcmi->in_r2 = VITAKI_CTRL_IN_REARTOUCH_RIGHT_R1;

  vcmi->did_init = true;
}
