// Very very simple homegrown immediate mode GUI
#include <sys/param.h>
#include <stdio.h>
#include <string.h>
#include <vita2d.h>
#include <psp2/ctrl.h>
#include <psp2/message_dialog.h>
#include <psp2/registrymgr.h> 
#include <psp2/ime_dialog.h>
#include <chiaki/base64.h>

#include "context.h"
#include "host.h"
#include "ui.h"
#include "util.h"

#define COLOR_WHITE RGBA8(255, 255, 255, 255)
#define COLOR_GRAY50 RGBA8(129, 129, 129, 255)
#define COLOR_BLACK RGBA8(0, 0, 0, 255)
#define COLOR_ACTIVE RGBA8(255, 170, 238, 255)
#define COLOR_TILE_BG RGBA8(51, 51, 51, 255)
#define COLOR_BANNER RGBA8(22, 45, 80, 255)

#define VITA_WIDTH 960
#define VITA_HEIGHT 544

#define HEADER_BAR_X 136
#define HEADER_BAR_Y 45
#define HEADER_BAR_H 26
#define HEADER_BAR_W 774
#define HOST_SLOTS_X HEADER_BAR_X - 86
#define HOST_SLOTS_Y HEADER_BAR_Y + HEADER_BAR_H + 45
#define HOST_SLOT_W 400
#define HOST_SLOT_H 200

#define TEXTURE_PATH "app0:/assets/"
#define BTN_REGISTER_PATH TEXTURE_PATH "btn_register.png"
#define BTN_REGISTER_ACTIVE_PATH TEXTURE_PATH "btn_register_active.png"
#define BTN_ADD_PATH TEXTURE_PATH "btn_add.png"
#define BTN_ADD_ACTIVE_PATH TEXTURE_PATH "btn_add_active.png"
#define BTN_DISCOVERY_PATH TEXTURE_PATH "btn_discovery.png"
#define BTN_DISCOVERY_ACTIVE_PATH TEXTURE_PATH "btn_discovery_active.png"
#define BTN_DISCOVERY_OFF_PATH TEXTURE_PATH "btn_discovery_off.png"
#define BTN_DISCOVERY_OFF_ACTIVE_PATH \
  TEXTURE_PATH "btn_discovery_off_active.png"
#define BTN_SETTINGS_PATH TEXTURE_PATH "btn_settings.png"
#define BTN_SETTINGS_ACTIVE_PATH TEXTURE_PATH "btn_settings_active.png"
#define BTN_MESSAGES_PATH TEXTURE_PATH "btn_messages.png"
#define BTN_MESSAGES_ACTIVE_PATH TEXTURE_PATH "btn_messages_active.png"
#define IMG_PS4_PATH TEXTURE_PATH "ps4.png"
#define IMG_PS4_OFF_PATH TEXTURE_PATH "ps4_off.png"
#define IMG_PS4_REST_PATH TEXTURE_PATH "ps4_rest.png"
#define IMG_PS5_PATH TEXTURE_PATH "ps5.png"
#define IMG_PS5_OFF_PATH TEXTURE_PATH "ps5_off.png"
#define IMG_PS5_REST_PATH TEXTURE_PATH "ps5_rest.png"
#define IMG_DISCOVERY_HOST TEXTURE_PATH "discovered_host.png"
#define IMG_HEADER_LOGO_PATH TEXTURE_PATH "header_logo.png"

vita2d_font* font;
vita2d_font* font_mono;
vita2d_texture *btn_register, *btn_register_active, *btn_add, *btn_add_active,
    *btn_discovery, *btn_discovery_active, *btn_discovery_off,
    *btn_discovery_off_active, *btn_settings, *btn_settings_active,
    *btn_messages, *btn_messages_active, *img_ps4,
    *img_ps4_off, *img_ps4_rest, *img_ps5, *img_ps5_off, *img_ps5_rest,
    *img_header, *img_discovery_host;

/// Identifiers of various widgets on the screen
typedef enum ui_main_widget_id_t {
  UI_MAIN_WIDGET_ADD_HOST_BTN,
  UI_MAIN_WIDGET_REGISTER_BTN,
  UI_MAIN_WIDGET_DISCOVERY_BTN,
  UI_MAIN_WIDGET_MESSAGES_BTN,
  UI_MAIN_WIDGET_SETTINGS_BTN,

  // needs to bitwise mask with up to 4 items (current max host count), so >=2 bits (may be increased in the future), and 4 is already occupied by MESSAGES_BTN
  UI_MAIN_WIDGET_HOST_TILE = 1 << 3,

  // FIXME: this is bound to fail REALLY fast if we start adding more inputs in the future
  UI_MAIN_WIDGET_TEXT_INPUT = 1 << 6,
} MainWidgetId;

/// Types of actions that can be performed on hosts
typedef enum ui_host_action_t {
  UI_HOST_ACTION_NONE = 0,
  UI_HOST_ACTION_WAKEUP,  // Only for at-rest hosts
  UI_HOST_ACTION_STREAM,  // Only for online hosts
  UI_HOST_ACTION_DELETE,  // Only for manually added hosts
  UI_HOST_ACTION_EDIT,    // Only for registered/manually added hosts
  UI_HOST_ACTION_REGISTER,    // Only for discovered hosts
} UIHostAction;

/// Types of screens that can be rendered
typedef enum ui_screen_type_t {
  UI_SCREEN_TYPE_MAIN = 0,
  UI_SCREEN_TYPE_REGISTER,
  UI_SCREEN_TYPE_REGISTER_HOST,
  UI_SCREEN_TYPE_ADD_HOST,
  UI_SCREEN_TYPE_EDIT_HOST,
  UI_SCREEN_TYPE_STREAM,
  UI_SCREEN_TYPE_SETTINGS,
  UI_SCREEN_TYPE_MESSAGES,
} UIScreenType;

/// Check if a button has been newly pressed
bool btn_pressed(SceCtrlButtons btn) {
  return (context.ui_state.button_state & btn) &&
         !(context.ui_state.old_button_state & btn);
}

/// Load all textures required for rendering the UI
void load_textures() {
  btn_add = vita2d_load_PNG_file(BTN_ADD_PATH);
  btn_add_active = vita2d_load_PNG_file(BTN_ADD_ACTIVE_PATH);
  btn_discovery = vita2d_load_PNG_file(BTN_DISCOVERY_PATH);
  btn_discovery_active = vita2d_load_PNG_file(BTN_DISCOVERY_ACTIVE_PATH);
  btn_discovery_off = vita2d_load_PNG_file(BTN_DISCOVERY_OFF_PATH);
  btn_discovery_off_active =
      vita2d_load_PNG_file(BTN_DISCOVERY_OFF_ACTIVE_PATH);
  btn_register = vita2d_load_PNG_file(BTN_REGISTER_PATH);
  btn_register_active = vita2d_load_PNG_file(BTN_REGISTER_ACTIVE_PATH);
  btn_settings = vita2d_load_PNG_file(BTN_SETTINGS_PATH);
  btn_settings_active = vita2d_load_PNG_file(BTN_SETTINGS_ACTIVE_PATH);
  btn_messages = vita2d_load_PNG_file(BTN_MESSAGES_PATH);
  btn_messages_active = vita2d_load_PNG_file(BTN_MESSAGES_ACTIVE_PATH);
  img_header = vita2d_load_PNG_file(IMG_HEADER_LOGO_PATH);
  img_ps4 = vita2d_load_PNG_file(IMG_PS4_PATH);
  img_ps4_off = vita2d_load_PNG_file(IMG_PS4_OFF_PATH);
  img_ps4_rest = vita2d_load_PNG_file(IMG_PS4_REST_PATH);
  img_ps5 = vita2d_load_PNG_file(IMG_PS5_PATH);
  img_ps4_off = vita2d_load_PNG_file(IMG_PS4_OFF_PATH);
  img_ps5_rest = vita2d_load_PNG_file(IMG_PS5_REST_PATH);
  img_discovery_host = vita2d_load_PNG_file(IMG_DISCOVERY_HOST);
}

/// Check if a given region is touched on the front touch screen
bool is_touched(int x, int y, int width, int height) {
  SceTouchData* tdf = &(context.ui_state.touch_state_front);
  if (!tdf) {
    return false;
  }
  // TODO: Do the coordinate systems really match?
  return tdf->report->x > x && tdf->report->x <= x + width &&
         tdf->report->y > y && tdf->report->y <= y + height;
}

/// Draw the tile for a host
/// @return The action to take for the host
UIHostAction host_tile(int host_slot, VitaChiakiHost* host) {
  int active_id = context.ui_state.active_item;
  bool is_active = active_id == (UI_MAIN_WIDGET_HOST_TILE | host_slot);
  bool discovered = host->type & DISCOVERED;
  bool registered = host->type & REGISTERED;
  bool added = host->type & MANUALLY_ADDED;
  bool mutable = (added || registered);
  bool at_rest = discovered && host->discovery_state->state ==
                                   CHIAKI_DISCOVERY_HOST_STATE_STANDBY;

  int x = HOST_SLOTS_X + (host_slot % 2) * (HOST_SLOT_W + 58);
  int y = HOST_SLOTS_Y;
  if (host_slot > 1) {
    y += HOST_SLOT_H + 11;
  }
  if (is_active) {
    vita2d_draw_rectangle(x - 3, y - 3, HOST_SLOT_W + 6, HOST_SLOT_H + 6,
                          COLOR_ACTIVE);
  }
  vita2d_draw_rectangle(x, y, HOST_SLOT_W, HOST_SLOT_H, COLOR_TILE_BG);

  // Draw host name and host id
  if (discovered) {
    vita2d_draw_texture(img_discovery_host, x, y);
    vita2d_font_draw_text(font, x + 68, y + 40, COLOR_WHITE, 40,
                         host->discovery_state->host_name);
    vita2d_font_draw_text(font, x + 255, y + 23, COLOR_WHITE, 20,
                         host->discovery_state->host_id);
  } else if (registered) {
    uint8_t* host_mac = host->registered_state->server_mac;
    vita2d_font_draw_textf(font, x + 68, y + 40, COLOR_WHITE, 32,
                          "%X%X%X%X%X%X", host_mac[0], host_mac[1], host_mac[2],
                          host_mac[3], host_mac[4], host_mac[5]);
    vita2d_font_draw_text(font, x + 255, y + 23, COLOR_WHITE, 20,
                         host->discovery_state->host_id);
  }

  // Draw host address
  vita2d_font_draw_text(font, x + 260, y + HOST_SLOT_H - 10, COLOR_WHITE, 20, host->hostname);

  vita2d_texture* console_img;
  bool is_ps5 = chiaki_target_is_ps5(host->target);
  // TODO: Don't use separate textures for off/on/rest, use tinting instead
  if (added && !discovered) {
    console_img = is_ps5 ? img_ps5 : img_ps4;
  } else if (at_rest) {
    console_img = is_ps5 ? img_ps5_rest : img_ps4_rest;
  } else {
    console_img = is_ps5 ? img_ps5 : img_ps4;
  }
  vita2d_draw_texture(console_img, x + 64, y + 64);
  if (discovered && !at_rest) {
    const char* app_name = host->discovery_state->running_app_name;
    const char* app_id = host->discovery_state->running_app_titleid;
    // printf("%s", app_name);
    // printf("%s", app_id);
    if (app_name && app_id) {
      vita2d_font_draw_text(font, x + 32, y + 16, COLOR_WHITE, 16, app_name);
      vita2d_font_draw_text(font, x + 300, y + 170, COLOR_WHITE, 16, app_id);
    }
  }

  // Handle navigation
  int btn = context.ui_state.button_state;
  int old_btn = context.ui_state.old_button_state;
  int last_slot = context.num_hosts - 1;
  if (is_active) {
    if (context.active_host != host) {
      context.active_host = host;
    }
    if (btn_pressed(SCE_CTRL_UP)) {
      if (host_slot < 2) {
        // Set focus on the last button of the header bar
        context.ui_state.next_active_item = UI_MAIN_WIDGET_SETTINGS_BTN;
      } else {
        // Set focus on the host tile directly above
        context.ui_state.next_active_item =
            UI_MAIN_WIDGET_HOST_TILE | (host_slot - 2);
      }
    } else if (btn_pressed(SCE_CTRL_RIGHT)) {
      if (host_slot != last_slot && (host_slot == 0 || host_slot == 2)) {
        // Set focus on the host tile to the right
        context.ui_state.next_active_item =
            UI_MAIN_WIDGET_HOST_TILE | (host_slot + 1);
      }
    } else if (btn_pressed(SCE_CTRL_DOWN)) {
      if (last_slot >= host_slot + 2 && host_slot < 2) {
        // Set focus on the host tile directly below
        context.ui_state.next_active_item =
            UI_MAIN_WIDGET_SETTINGS_BTN | (host_slot + 2);
      }
    } else if (btn_pressed(SCE_CTRL_LEFT)) {
      if (host_slot == 1 || host_slot == 3) {
        context.ui_state.next_active_item =
            UI_MAIN_WIDGET_HOST_TILE | (host_slot - 1);
      }
    }
    // Determine action to perform
    // if (btn_pressed(SCE_CTRL_CROSS) && !registered) {
    //   for (int i = 0; i < context.config.num_registered_hosts; i++) {
    //     printf("0x%x", context.config.registered_hosts[i]->registered_state);
    //     if (context.config.registered_hosts[i] != NULL && strcmp(context.active_host->hostname, context.config.registered_hosts[i]->hostname) == 0) {
    //       context.active_host->registered_state = context.config.registered_hosts[i]->registered_state;
    //       context.active_host->type |= REGISTERED;
    //       registered = true;
    //       break;
    //     }
    //   }
    // }
    // if (btn_pressed(SCE_CTRL_CROSS) && !registered && !added) {
    //   for (int i = 0; i < context.config.num_manual_hosts; i++) {
    //     if (context.config.manual_hosts[i] != NULL && strcmp(context.active_host->hostname, context.config.manual_hosts[i]->hostname) == 0) {
    //       context.active_host->registered_state = context.config.manual_hosts[i]->registered_state;
    //       context.active_host->type |= MANUALLY_ADDED;
    //       context.active_host->type |= REGISTERED;
    //       added = true;
    //       break;
    //     }
    //   }
    // }
    if (registered && btn_pressed(SCE_CTRL_CROSS)) {
      if (host->discovery_state->state == CHIAKI_DISCOVERY_HOST_STATE_STANDBY) {
        return UI_HOST_ACTION_WAKEUP;
      } else {
        vita2d_end_drawing();
        vita2d_common_dialog_update();
        vita2d_swap_buffers();
        host_stream(context.active_host);
        return UI_HOST_ACTION_STREAM;
      }
    } else if (!registered && !added && discovered && btn_pressed(SCE_CTRL_CROSS)){
      return UI_HOST_ACTION_REGISTER;
    } else if (mutable && btn_pressed(SCE_CTRL_TRIANGLE)) {
      return UI_HOST_ACTION_DELETE;
    } else if (mutable && btn_pressed(SCE_CTRL_SQUARE)) {
      return UI_HOST_ACTION_EDIT;
    }
  }
  if (is_touched(x, y, HOST_SLOT_W, HOST_SLOT_H)) {
    context.ui_state.next_active_item = UI_MAIN_WIDGET_HOST_TILE | host_slot;
  }
  return UI_HOST_ACTION_NONE;
}

/// Draw a button into the header bar
/// @param id           Numerical identifier of the button
/// @param x_offset     Offset on the X axis in pixels from the start of the
///                     header bar
/// @param default_img  Image to render when the button is not active
/// @param active_img   Image to render when the button is active
/// @return whether the button is pressed or not
bool header_button(MainWidgetId id, int x_offset, vita2d_texture* default_img,
                   vita2d_texture* active_img) {
  int active_id = context.ui_state.active_item;
  bool is_active = active_id == id;

  // Draw button
  vita2d_texture* img = is_active ? active_img : default_img;
  int w = vita2d_texture_get_width(img);
  int h = vita2d_texture_get_height(img);
  int x = HEADER_BAR_X + x_offset;
  // Buttons are bottom-aligned to the header bar
  int y = HEADER_BAR_Y + HEADER_BAR_H - h;
  vita2d_draw_texture(img, x, y);

  // Navigation handling
  int btn = context.ui_state.button_state;
  if (is_active) {
    if (btn_pressed(SCE_CTRL_DOWN) && context.num_hosts > 0) {
      // Select first host tile
      context.ui_state.next_active_item = UI_MAIN_WIDGET_HOST_TILE;
    } else if (btn_pressed(SCE_CTRL_LEFT)) {
      // Select button to the left
      context.ui_state.next_active_item = MAX(0, active_id - 1);
    } else if (btn_pressed(SCE_CTRL_RIGHT)) {
      // Select button to the right
      context.ui_state.next_active_item =
          MIN(UI_MAIN_WIDGET_SETTINGS_BTN, active_id + 1);
    }
  }

  if (is_touched(x, y, w, h)) {
    // Focus the button if it's touched, no matter the button state
    context.ui_state.next_active_item = id;
    return true;
  }
  return is_active && btn_pressed(SCE_CTRL_CROSS);
}
uint16_t IMEInput[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1];
bool showingIME = false;
char* text_input(MainWidgetId id, int x, int y, int w, int h, char* label,
                char* value, int max_len) {
  bool is_active = context.ui_state.active_item == id;
  if (is_active) {
    vita2d_draw_rectangle(x + 300 - 3, y - 3, w + 6, h + 6, COLOR_ACTIVE);
    if (btn_pressed(SCE_CTRL_CROSS)) {
      SceImeDialogParam param;

      sceImeDialogParamInit(&param);
			param.supportedLanguages = SCE_IME_LANGUAGE_ENGLISH;
			param.languagesForced = SCE_TRUE;
			param.type = SCE_IME_TYPE_DEFAULT;
			param.option = SCE_IME_OPTION_NO_ASSISTANCE;
			param.textBoxMode = SCE_IME_DIALOG_TEXTBOX_MODE_DEFAULT;
      uint16_t IMELabel[label != NULL ? sizeof(label) + 1 : sizeof("Text")];
      utf8_to_utf16(label != NULL ? label : "Text", IMELabel);
			param.title = IMELabel;
			param.maxTextLength = SCE_IME_DIALOG_MAX_TEXT_LENGTH;
      if (value != NULL) {
        uint16_t IMEValue[sizeof(value)];
        utf8_to_utf16(value, IMEValue);
			  param.initialText = IMEValue;
      }
			param.inputTextBuffer = IMEInput;

      showingIME = true;
      sceImeDialogInit(&param);
    }
  }
  vita2d_draw_rectangle(x + 300, y, w, h, COLOR_BLACK);
  // vita2d_draw_texture(icon, x + 20, y + h / 2);
  if (label != NULL) vita2d_font_draw_text(font, x, y + h / 1.5, COLOR_WHITE, 40, label);
  if (value != NULL) vita2d_font_draw_text(font, x + 300, y + h / 1.5, COLOR_WHITE, 40, value);
  
  if (is_active && showingIME) {
    if (sceImeDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_FINISHED) {
      showingIME = false;
      SceImeDialogResult result={};
      sceImeDialogGetResult(&result);
      sceImeDialogTerm();
      if (result.button == SCE_IME_DIALOG_BUTTON_ENTER) {

        uint16_t*last_input = (result.button == SCE_IME_DIALOG_BUTTON_ENTER) ? IMEInput:u"";
        char IMEResult[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1];
        utf16_to_utf8(IMEInput, IMEResult);
        LOGD("IME returned %s", IMEResult);
        return strdup(IMEResult);
      }
    }
  }
  return NULL;

  // TODO: Render label + icon
  // TODO: Render input border
  // TODO: Render value
  // TODO: If touched or X pressed, open up IME dialogue and update value
}

int number_input(MainWidgetId id, int x, int y, int w, int h, char* label, int value) {

  // int to str
  char value_str[100];
  snprintf(value_str, 100, "%d", value);

  bool is_active = context.ui_state.active_item == id;
  if (is_active) {
    vita2d_draw_rectangle(x + 300 - 3, y - 3, w + 6, h + 6, COLOR_ACTIVE);
    if (btn_pressed(SCE_CTRL_CROSS)) {
      SceImeDialogParam param;

      sceImeDialogParamInit(&param);
			param.supportedLanguages = SCE_IME_LANGUAGE_ENGLISH;
			param.languagesForced = SCE_TRUE;
			param.type = SCE_IME_TYPE_NUMBER;
			param.option = SCE_IME_OPTION_NO_ASSISTANCE;
			param.textBoxMode = SCE_IME_DIALOG_TEXTBOX_MODE_DEFAULT;
      uint16_t IMELabel[label != NULL ? sizeof(label) + 1 : sizeof("Text")];
      utf8_to_utf16(label != NULL ? label : "Text", IMELabel);
			param.title = IMELabel;
			param.maxTextLength = SCE_IME_DIALOG_MAX_TEXT_LENGTH;
      uint16_t IMEValue[sizeof(value_str)];
      utf8_to_utf16(value_str, IMEValue);
      param.initialText = IMEValue;
			param.inputTextBuffer = IMEInput;

      showingIME = true;
      sceImeDialogInit(&param);
    }
  }
  vita2d_draw_rectangle(x + 300, y, w, h, COLOR_BLACK);
  // vita2d_draw_texture(icon, x + 20, y + h / 2);
  if (label != NULL) vita2d_font_draw_text(font, x, y + h / 1.5, COLOR_WHITE, 40, label);
  vita2d_font_draw_text(font, x + 300, y + h / 1.5, COLOR_WHITE, 40, value_str);

  if (is_active && showingIME) {
    if (sceImeDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_FINISHED) {
      showingIME = false;
      SceImeDialogResult result={};
      sceImeDialogGetResult(&result);
      sceImeDialogTerm();
      if (result.button == SCE_IME_DIALOG_BUTTON_ENTER) {

        uint16_t*last_input = (result.button == SCE_IME_DIALOG_BUTTON_ENTER) ? IMEInput:u"";
        char IMEResult[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1];
        utf16_to_utf8(IMEInput, IMEResult);
        LOGD("IME returned %s", IMEResult);
        return strtoimax(strdup(IMEResult), NULL, 10);
      }
    }
  }

  // TODO: Render label + icon
  // TODO: Render input border
  // TODO: Render value
  // TODO: If touched or X pressed, open up IME dialogue and return value
  return -1;
}

int choice_input(int x, int y, int w, int h, char* label, vita2d_texture* icon,
                 char** choice_labels, size_t num_choices, uint8_t cur_choice) {
  // TODO: Render label + icon
  // TODO: Render input border
  // TODO: Render value
  // TODO: Button/touch handling to update choice
  return -1;
}

void load_psn_id_if_needed() {
  if (context.config.psn_account_id == NULL || strlen(context.config.psn_account_id) < 1) {
    char accIDBuf[8];
    memset(accIDBuf, 0, sizeof(accIDBuf));
    free(context.config.psn_account_id);
    context.config.psn_account_id = (char*)malloc(get_base64_size(sizeof(accIDBuf))+1); // WHY IS +1 NEEDED HERE ONLY WHEN RUN ON THE FIRST FRAME BECAUSE ITS 14 THEN FOR SOME REASON AND ADDING 1 MAKES IT 12????????? BUT THEN EVERY TIME AFTER IT REMAINS 12 DESPITE ADDING 1 SO IT SOMEHOW WORKS?????????????????????????????????????
    sceRegMgrGetKeyBin("/CONFIG/NP/", "account_id", accIDBuf, sizeof(accIDBuf));
    chiaki_base64_encode(accIDBuf, sizeof(accIDBuf), context.config.psn_account_id, get_base64_size(sizeof(accIDBuf)));
    LOGD("size of id %d", strlen(context.config.psn_account_id));
  }
}

/// Draw the header bar for the main menu screen
/// @return the screen to draw during the next cycle
UIScreenType draw_header_bar() {
  // Header background
  // FIXME: What's the performance impact of this? Should we define this as
  // constants?
  int w = vita2d_texture_get_width(img_header);
  int h = vita2d_texture_get_height(img_header);
  vita2d_draw_texture(img_header, HEADER_BAR_X - w,
                      HEADER_BAR_Y - (h - HEADER_BAR_H) / 2);
  vita2d_draw_rectangle(HEADER_BAR_X, HEADER_BAR_Y, HEADER_BAR_W, HEADER_BAR_H,
                        COLOR_BANNER);

  // Header buttons
  UIScreenType next_screen = UI_SCREEN_TYPE_MAIN;
  if (header_button(UI_MAIN_WIDGET_ADD_HOST_BTN, 315, btn_add,
                    btn_add_active)) {
    next_screen = UI_SCREEN_TYPE_ADD_HOST;
  }
  if (header_button(UI_MAIN_WIDGET_REGISTER_BTN, 475, btn_register,
                    btn_register_active)) {
    // TODO what was this button supposed to do??
    //next_screen = UI_SCREEN_TYPE_REGISTER;
  }
  bool discovery_on = context.discovery_enabled;
  if (header_button(
          UI_MAIN_WIDGET_DISCOVERY_BTN, 639,
          discovery_on ? btn_discovery : btn_discovery_off,
          discovery_on ? btn_discovery_active : btn_discovery_off_active)) {
    if (discovery_on) {
      stop_discovery();
    } else {
      start_discovery(NULL, NULL);
    }
  }
  if (header_button(UI_MAIN_WIDGET_MESSAGES_BTN, 684, btn_messages,
                    btn_messages_active)) {
    next_screen = UI_SCREEN_TYPE_MESSAGES;
  }
  if (header_button(UI_MAIN_WIDGET_SETTINGS_BTN, 729, btn_settings,
                    btn_settings_active)) {
    next_screen = UI_SCREEN_TYPE_SETTINGS;
  }
  return next_screen;
}

/// Draw the main menu screen with the list of hosts and header bar
/// @return the screen to draw during the next cycle
UIScreenType draw_main_menu() {
  UIScreenType next_screen = draw_header_bar();

  int host_slots = 0;
  UIHostAction host_action = UI_HOST_ACTION_NONE;
  for (int i = 0; i < MAX_NUM_HOSTS; i++) {
    VitaChiakiHost* host = context.hosts[i];
    if (!host) {
      continue;
    }
    host_action = host_tile(host_slots, host);
    host_slots++;
  }
  if (host_slots == 0) {
    // TODO: Draw a "Please add a host via the header bar" message
    //       in the center of the screen, maybe with a nice little arrow image?
  } else if (host_action == UI_HOST_ACTION_WAKEUP) {
    host_wakeup(context.active_host);
  } else if (host_action == UI_HOST_ACTION_STREAM) {
    next_screen = UI_SCREEN_TYPE_MESSAGES;
  } else if (host_action == UI_HOST_ACTION_EDIT) {
    next_screen = UI_SCREEN_TYPE_REGISTER_HOST;
    // next_screen = UI_SCREEN_TYPE_EDIT_HOST;
  } else if (host_action == UI_HOST_ACTION_REGISTER) {
    next_screen = UI_SCREEN_TYPE_REGISTER_HOST;
  }


  // Add "tooltip" in bottom of screen
  int font_size = 18;
  int tooltip_x = 10;
  int tooltip_y = VITA_HEIGHT - font_size;
  char* tooltip_msg = "";

  switch (context.ui_state.active_item) {
  case UI_MAIN_WIDGET_ADD_HOST_BTN:
    tooltip_msg = "Manually add host (functionality currently disabled; wait for discovery instead)";
    break;
  case UI_MAIN_WIDGET_REGISTER_BTN:
    tooltip_msg = "Manually register (functionality currently disabled)";
    break;
  case UI_MAIN_WIDGET_DISCOVERY_BTN:
    tooltip_msg = (context.discovery_enabled) ? "Turn off discovery" : "Turn on discovery";
    break;
  case UI_MAIN_WIDGET_MESSAGES_BTN:
    tooltip_msg = "View log";
    break;
  case UI_MAIN_WIDGET_SETTINGS_BTN:
    tooltip_msg = "Settings";
    break;
  default:
    tooltip_msg = "";
  }

  if (strlen(tooltip_msg)) {
    vita2d_font_draw_text(font, tooltip_x, tooltip_y,
                          COLOR_WHITE, font_size,
                          tooltip_msg
                          );
  }


  return next_screen;
}
char* PSNID_LABEL = "PSN ID";
char* CONTROLLER_MAP_ID_LABEL = "Controller map";
/// Draw the settings form
/// @return whether the dialog should keep rendering
bool draw_settings() {
  char* psntext = text_input(UI_MAIN_WIDGET_TEXT_INPUT | 1, 30, 30, 600, 80, PSNID_LABEL, context.config.psn_account_id, 20);
  if (psntext != NULL) {
    // LOGD("psntext is %s", psntext);
    free(context.config.psn_account_id);
    context.config.psn_account_id = psntext;
    load_psn_id_if_needed();
    config_serialize(&context.config);
  }

  int ctrlmap_id = number_input(UI_MAIN_WIDGET_TEXT_INPUT | 2, 30, 140, 600, 80, CONTROLLER_MAP_ID_LABEL, context.config.controller_map_id);
  if (ctrlmap_id != -1) {
    // LOGD("ctrlmap_id is %d", ctrlmap_id);
    context.config.controller_map_id = ctrlmap_id;
    config_serialize(&context.config);
  }

  // Draw controller text notes
  int font_size = 18;
  int info_x = 30;
  int info_y = 250;
  int info_y_delta = 21;

  vita2d_font_draw_text(font, info_x, info_y, COLOR_WHITE, font_size,
                        "Controller map values: 0,1,2,3,4,5,6,7,25: official remote play maps (see vs0:app/NPXS10013/keymap/)"
                        );
  vita2d_font_draw_text(font, info_x, info_y + 1*info_y_delta, COLOR_WHITE, font_size,
                        "0: L2, R2 rear upper quarters; L3, R3 rear lower quarters; touchpad entire front"
                        );
  vita2d_font_draw_text(font, info_x, info_y + 2*info_y_delta, COLOR_WHITE, font_size,
                        "1: L2, R2 front upper corners; L3, R3 front lower corners; touchpad front center"
                        );
  vita2d_font_draw_text(font, info_x, info_y + 3*info_y_delta, COLOR_WHITE, font_size,
                        "2: L2, R2 front lower corners; L3, R3 rear left/right half; touchpad front center"
                        );
  vita2d_font_draw_text(font, info_x, info_y + 4*info_y_delta, COLOR_WHITE, font_size,
                        "3: L2, R2 front upper corners; L3, R3 rear left/right half; touchpad front center"
                        );
  vita2d_font_draw_text(font, info_x, info_y + 5*info_y_delta, COLOR_WHITE, font_size,
                        "4: No L2, R2, L3, R3; touchpad entire front"
                        );
  vita2d_font_draw_text(font, info_x, info_y + 6*info_y_delta, COLOR_WHITE, font_size,
                        "5: No L2, R2, L3, R3, or touchpad"
                        );
  vita2d_font_draw_text(font, info_x, info_y + 7*info_y_delta, COLOR_WHITE, font_size,
                        "6: L2, R2 front lower corners; no L3, R3; touchpad front center"
                        );
  vita2d_font_draw_text(font, info_x, info_y + 8*info_y_delta, COLOR_WHITE, font_size,
                        "7: L2, R2 front upper corners; no L3, R3; touchpad front center"
                        );
  vita2d_font_draw_text(font, info_x, info_y + 9*info_y_delta, COLOR_WHITE, font_size,
                        "25: L2, R2 front upper corners; L3, R3 front lower corners; no touchpad"
                        );
  vita2d_font_draw_text(font, info_x, info_y + 10*info_y_delta, COLOR_WHITE, font_size,
                        "99: L2, R2 = L1 + rear, R1 + rear; L3 = Left+Square, R3 = Right+Circle; touchpad entire front"
                        );
  vita2d_font_draw_text(font, info_x, info_y + 12*info_y_delta, COLOR_WHITE, font_size,
                        "In all maps, press Start + Select simultaneously for PS (home) button"
                        );

  if (btn_pressed(SCE_CTRL_DOWN)) {
    context.ui_state.next_active_item = (UI_MAIN_WIDGET_TEXT_INPUT | 2);
  }
  if (btn_pressed(SCE_CTRL_UP)) {
    context.ui_state.next_active_item = (UI_MAIN_WIDGET_TEXT_INPUT | 1);
  }

  if (btn_pressed(SCE_CTRL_CIRCLE)) {
    context.ui_state.next_active_item = UI_MAIN_WIDGET_SETTINGS_BTN;
    // free(context.config.psn_account_id);
    // context.config.psn_account_id = NULL;
    return false;
  }
  return true;
}

char* LINK_CODE;
char* LINK_CODE_LABEL = "Registration code";

/// Draw the form to register a host
/// @return whether the dialog should keep rendering
bool draw_registration_dialog() { 
  char* text = text_input(UI_MAIN_WIDGET_TEXT_INPUT | 0, 30, 30, 600, 80, LINK_CODE_LABEL, LINK_CODE, 8);
  if (text != NULL) {
    if (LINK_CODE != NULL) free(LINK_CODE);
    LINK_CODE = text;
    }
  if (btn_pressed(SCE_CTRL_CIRCLE)) {
    if ((LINK_CODE != NULL) && (strlen(LINK_CODE) != 0)) {
      LOGD("User input link code: %s", LINK_CODE);
      host_register(context.active_host, atoi(LINK_CODE));
    } else {
      LOGD("User exited registration screen without inputting link code");
    }
    context.ui_state.next_active_item = UI_MAIN_WIDGET_SETTINGS_BTN;
    // free(context.config.psn_account_id);
    // context.config.psn_account_id = NULL;
    return false;
  }
  return true;
}
/// Draw the form to manually add a new host
/// @return whether the dialog should keep rendering
bool draw_add_host_dialog() { 


  return false; 
}

/// Draw the form to edit an existing host
/// @return whether the dialog should keep rendering
bool draw_edit_host_dialog() { 

  return false;
}
/// Render the current frame of an active stream
/// @return whether the stream should keep rendering
bool draw_stream() { 
  if (context.stream.is_streaming) context.stream.is_streaming = false;
  
  return false;
}

/// Draw the debug messages screen
/// @return whether the dialog should keep rendering
bool draw_messages() {
  vita2d_set_clear_color(RGBA8(0x00, 0x00, 0x00, 0xFF));
  context.ui_state.next_active_item = -1;

  // initialize mlog_line_offset
  if (!context.ui_state.mlog_last_update) context.ui_state.mlog_line_offset = -1;
  if (context.ui_state.mlog_last_update != context.mlog->last_update) {
    context.ui_state.mlog_last_update = context.mlog->last_update;
    context.ui_state.mlog_line_offset = -1;
  }


  int w = VITA_WIDTH;
  int h = VITA_HEIGHT;

  int left_margin = 12;
  int top_margin = 20;
  int bottom_margin = 20;
  int font_size = 18;
  int line_height = font_size + 2;

  // compute lines to print
  // TODO enable scrolling etc
  int max_lines = (h - top_margin - bottom_margin) / line_height;
  bool overflow = (context.mlog->lines > max_lines);

  int max_line_offset = 0;
  if (overflow) {
    max_line_offset = context.mlog->lines - max_lines + 1;
  } else {
    max_line_offset = 0;
    context.ui_state.mlog_line_offset = -1;
  }
  int line_offset = max_line_offset;

  // update line offset according to mlog_line_offset
  if (context.ui_state.mlog_line_offset >= 0) {
    if (context.ui_state.mlog_line_offset <= max_line_offset) {
      line_offset = context.ui_state.mlog_line_offset;
    }
  }

  int y = top_margin;
  int i_y = 0;
  if (overflow && (line_offset > 0)) {
    char note[100];
    if (line_offset == 1) {
      snprintf(note, 100, "<%d line above>", line_offset);
    } else {
      snprintf(note, 100, "<%d lines above>", line_offset);
    }
    vita2d_font_draw_text(font_mono, left_margin, y,
                          COLOR_GRAY50, font_size,
                          note
                          );
    y += line_height;
    i_y ++;
  }

  int j;
  for (j = line_offset; j < context.mlog->lines; j++) {
    if (i_y > max_lines - 1) break;
    if (overflow && (i_y == max_lines - 1)) {
      if (j < context.mlog->lines - 1) break;
    }
    vita2d_font_draw_text(font_mono, left_margin, y,
                          COLOR_WHITE, font_size,
                          get_message_log_line(context.mlog, j)
                          );
    y += line_height;
    i_y ++;
  }
  if (overflow && (j < context.mlog->lines - 1)) {
    char note[100];
    int lines_below = context.mlog->lines - j - 1;
    if (lines_below == 1) {
      snprintf(note, 100, "<%d line below>", lines_below);
    } else {
      snprintf(note, 100, "<%d lines below>", lines_below);
    }
    vita2d_font_draw_text(font_mono, left_margin, y,
                          COLOR_GRAY50, font_size,
                          note
                          );
    y += line_height;
    i_y ++;
  }

  if (btn_pressed(SCE_CTRL_UP)) {
    if (overflow) {
      int next_offset = line_offset - 1;

      if (next_offset == 1) next_offset = 0;
      if (next_offset == max_line_offset-1) next_offset = max_line_offset-2;

      if (next_offset < 0) next_offset = line_offset;
      context.ui_state.mlog_line_offset = next_offset;
    }
  }
  if (btn_pressed(SCE_CTRL_DOWN)) {
    if (overflow) {
      int next_offset = line_offset + 1;

      if (next_offset == max_line_offset - 1) next_offset = max_line_offset;
      if (next_offset == 1) next_offset = 2;

      if (next_offset > max_line_offset) next_offset = max_line_offset;
      context.ui_state.mlog_line_offset = next_offset;
    }
  }

  if (btn_pressed(SCE_CTRL_CIRCLE)) {
    // TODO abort connection if connecting
    vita2d_set_clear_color(RGBA8(0x40, 0x40, 0x40, 0xFF));
    context.ui_state.next_active_item = UI_MAIN_WIDGET_MESSAGES_BTN;
    return false;
  }
  return true;
}

void init_ui() {
  vita2d_init();
  vita2d_set_clear_color(RGBA8(0x40, 0x40, 0x40, 0xFF));
  load_textures();
  font = vita2d_load_font_file("app0:/assets/fonts/Roboto-Regular.ttf");
  font_mono = vita2d_load_font_file("app0:/assets/fonts/RobotoMono-Regular.ttf");
  vita2d_set_vblank_wait(true);
}

/// Main UI loop
void draw_ui() {
  init_ui();
  SceCtrlData ctrl;
  memset(&ctrl, 0, sizeof(ctrl));


  UIScreenType screen = UI_SCREEN_TYPE_MAIN;

  load_psn_id_if_needed();

  while (true) {
    // if (!context.stream.is_streaming) {
      // sceKernelDelayThread(1000 * 10);
      // Get current controller state
      if (!sceCtrlReadBufferPositive(0, &ctrl, 1)) {
        // Try again...
        LOGE("Failed to get controller state");
        continue;
      }
      context.ui_state.old_button_state = context.ui_state.button_state;
      context.ui_state.button_state = ctrl.buttons;

      // Get current touch state
      sceTouchPeek(SCE_TOUCH_PORT_FRONT, &(context.ui_state.touch_state_front),
                  1);

      if (context.ui_state.next_active_item >= 0) {
        context.ui_state.active_item = context.ui_state.next_active_item;
        context.ui_state.next_active_item = -1;
      }
      if (!context.stream.is_streaming) {
        vita2d_start_drawing();
        vita2d_clear_screen();

        // Render the current screen
        if (screen == UI_SCREEN_TYPE_MAIN) {
          screen = draw_main_menu();
        } else if (screen == UI_SCREEN_TYPE_REGISTER_HOST) {
          context.ui_state.next_active_item = (UI_MAIN_WIDGET_TEXT_INPUT | 0);
          if (!draw_registration_dialog()) {
            screen = UI_SCREEN_TYPE_MAIN;
          }
        } else if (screen == UI_SCREEN_TYPE_ADD_HOST) {
          if (!draw_add_host_dialog()) {
            screen = UI_SCREEN_TYPE_MAIN;
          }
        } else if (screen == UI_SCREEN_TYPE_EDIT_HOST) {
          if (!draw_edit_host_dialog()) {
            screen = UI_SCREEN_TYPE_MAIN;
          }
        } else if (screen == UI_SCREEN_TYPE_MESSAGES) {
          if (!draw_messages()) {
            screen = UI_SCREEN_TYPE_MAIN;
          }
        } else if (screen == UI_SCREEN_TYPE_STREAM) {
          if (!draw_stream()) {
            screen = UI_SCREEN_TYPE_MAIN;
          }
        } else if (screen == UI_SCREEN_TYPE_SETTINGS) {
          if (context.ui_state.active_item != (UI_MAIN_WIDGET_TEXT_INPUT | 2)) {
            context.ui_state.next_active_item = (UI_MAIN_WIDGET_TEXT_INPUT | 1);
          }
          if (!draw_settings()) {
            screen = UI_SCREEN_TYPE_MAIN;
          }
        }
        vita2d_end_drawing();
        vita2d_common_dialog_update();
        vita2d_swap_buffers();
      }
    // }
  }
}
