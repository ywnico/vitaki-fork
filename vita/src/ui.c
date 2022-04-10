// Very very simple homegrown immediate mode GUI
#include <sys/param.h>
#include <stdio.h>
#include <string.h>
#include <vita2d.h>
#include <psp2/ctrl.h>

#include "context.h"
#include "host.h"
#include "ui.h"

#define COLOR_WHITE RGBA8(255, 255, 255, 255)
#define COLOR_ACTIVE RGBA8(255, 170, 238, 255)
#define COLOR_TILE_BG RGBA8(51, 51, 51, 255)
#define COLOR_BANNER RGBA8(22, 45, 80, 255)

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
#define IMG_PS4_PATH TEXTURE_PATH "ps4.png"
#define IMG_PS4_OFF_PATH TEXTURE_PATH "ps4_off.png"
#define IMG_PS4_REST_PATH TEXTURE_PATH "ps4_rest.png"
#define IMG_PS5_PATH TEXTURE_PATH "ps5.png"
#define IMG_PS5_OFF_PATH TEXTURE_PATH "ps5_off.png"
#define IMG_PS5_REST_PATH TEXTURE_PATH "ps5_rest.png"
#define IMG_DISCOVERY_HOST TEXTURE_PATH "discovered_host.png"
#define IMG_HEADER_LOGO_PATH TEXTURE_PATH "header_logo.png"

vita2d_font* font;
vita2d_texture *btn_register, *btn_register_active, *btn_add, *btn_add_active,
    *btn_discovery, *btn_discovery_active, *btn_discovery_off,
    *btn_discovery_off_active, *btn_settings, *btn_settings_active, *img_ps4,
    *img_ps4_off, *img_ps4_rest, *img_ps5, *img_ps5_off, *img_ps5_rest,
    *img_header, *img_discovery_host;

/// Identifiers of various widgets on the screen
typedef enum ui_main_widget_id_t {
  UI_MAIN_WIDGET_ADD_HOST_BTN,
  UI_MAIN_WIDGET_REGISTER_BTN,
  UI_MAIN_WIDGET_DISCOVERY_BTN,
  UI_MAIN_WIDGET_SETTINGS_BTN,
  UI_MAIN_WIDGET_HOST_TILE,
} MainWidgetId;

/// Types of actions that can be performed on hosts
typedef enum ui_host_action_t {
  UI_HOST_ACTION_NONE = 0,
  UI_HOST_ACTION_WAKEUP,  // Only for at-rest hosts
  UI_HOST_ACTION_STREAM,  // Only for online hosts
  UI_HOST_ACTION_DELETE,  // Only for manually added hosts
  UI_HOST_ACTION_EDIT,    // Only for registered/manually added hosts
} UIHostAction;

/// Types of screens that can be rendered
typedef enum ui_screen_type_t {
  UI_SCREEN_TYPE_MAIN = 0,
  UI_SCREEN_TYPE_REGISTER,
  UI_SCREEN_TYPE_ADD_HOST,
  UI_SCREEN_TYPE_EDIT_HOST,
  UI_SCREEN_TYPE_STREAM,
  UI_SCREEN_TYPE_SETTINGS,
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
  vita2d_font_draw_text(font, x + 260, y + 44, COLOR_WHITE, 20, host->hostname);

  vita2d_texture* console_img;
  bool is_ps5 = chiaki_target_is_ps5(host->target);
  // TODO: Don't use separate textures for off/on/rest, use tinting instead
  if (added && !discovered) {
    console_img = is_ps5 ? img_ps5_off : img_ps4_off;
  } else if (at_rest) {
    console_img = is_ps5 ? img_ps5_rest : img_ps4_rest;
  } else {
    console_img = is_ps5 ? img_ps5_off : img_ps4_off;
  }
  vita2d_draw_texture(console_img, x + 64, y + 64);
  if (discovered && !at_rest) {
    const char* app_name = host->discovery_state->running_app_name;
    const char* app_id = host->discovery_state->running_app_titleid;
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
  }
  if (is_touched(x, y, HOST_SLOT_W, HOST_SLOT_H)) {
    context.ui_state.next_active_item = UI_MAIN_WIDGET_HOST_TILE | host_slot;
  }

  // Determine action to perform
  if (registered && btn_pressed(SCE_CTRL_CROSS)) {
    if (host->discovery_state->state == CHIAKI_DISCOVERY_HOST_STATE_STANDBY) {
      return UI_HOST_ACTION_WAKEUP;
    } else {
      return UI_HOST_ACTION_STREAM;
    }
  } else if (mutable && btn_pressed(SCE_CTRL_TRIANGLE)) {
    return UI_HOST_ACTION_DELETE;
  } else if (mutable && btn_pressed(SCE_CTRL_SQUARE)) {
    return UI_HOST_ACTION_EDIT;
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

void text_input(int x, int y, int w, int h, char* label, vita2d_texture* icon,
                char* value, int max_len) {
  // TODO: Render label + icon
  // TODO: Render input border
  // TODO: Render value
  // TODO: If touched or X pressed, open up IME dialogue and update value
}

int number_input(int x, int y, int w, int h, char* label,
                 vita2d_texture* icon) {
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
  if (header_button(UI_MAIN_WIDGET_ADD_HOST_BTN, 360, btn_add,
                    btn_add_active)) {
    next_screen = UI_SCREEN_TYPE_ADD_HOST;
  }
  if (header_button(UI_MAIN_WIDGET_REGISTER_BTN, 520, btn_register,
                    btn_register_active)) {
    next_screen = UI_SCREEN_TYPE_REGISTER;
  }
  bool discovery_on = context.discovery_enabled;
  if (header_button(
          UI_MAIN_WIDGET_DISCOVERY_BTN, 684,
          discovery_on ? btn_discovery : btn_discovery_off,
          discovery_on ? btn_discovery_active : btn_discovery_off_active)) {
    if (discovery_on) {
      stop_discovery();
    } else {
      start_discovery(NULL, NULL);
    }
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
  } else if (host_action == UI_HOST_ACTION_STREAM) {
    next_screen = UI_SCREEN_TYPE_STREAM;
  } else if (host_action == UI_HOST_ACTION_EDIT) {
    next_screen = UI_SCREEN_TYPE_EDIT_HOST;
  }
  return next_screen;
}

/// Draw the form to register a host
/// @return whether the dialog should keep rendering
bool draw_registration_dialog() { return false; }

/// Draw the form to manually add a new host
/// @return whether the dialog should keep rendering
bool draw_add_host_dialog() { return false; }

/// Draw the form to edit an existing host
/// @return whether the dialog should keep rendering
bool draw_edit_host_dialog() { return false; }

/// Render the current frame of an active stream
/// @return whether the stream should keep rendering
bool draw_stream() { return false; }

/// Main UI loop
void draw_ui() {
  vita2d_init();
  vita2d_set_clear_color(RGBA8(0x40, 0x40, 0x40, 0xFF));

  SceCtrlData ctrl;
  memset(&ctrl, 0, sizeof(ctrl));

  UIScreenType screen = UI_SCREEN_TYPE_MAIN;
  load_textures();
  font = vita2d_load_font_file("app0:/assets/fonts/Roboto-Regular.ttf");

  vita2d_set_vblank_wait(true);
  while (true) {
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

    vita2d_start_drawing();
    vita2d_clear_screen();

    // Render the current screen
    if (screen == UI_SCREEN_TYPE_MAIN) {
      screen = draw_main_menu();
    } else if (screen == UI_SCREEN_TYPE_REGISTER) {
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
    } else if (screen == UI_SCREEN_TYPE_STREAM) {
      if (!draw_stream()) {
        screen = UI_SCREEN_TYPE_MAIN;
      }
    }
    vita2d_end_drawing();
    vita2d_swap_buffers();
  }
}