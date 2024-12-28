#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
// #include <debugnet.h>

#include "stdlib.h"

#include "message_log.h"
#include "context.h"
#include "config.h"
#include "ui.h"

VitaChiakiContext context;

void log_cb_debugnet(ChiakiLogLevel lvl, const char *msg, void *user) {
  // int debugnet_lvl;
  // switch (lvl) {
  //   case CHIAKI_LOG_ERROR:
  //   case CHIAKI_LOG_WARNING:
  //     debugnet_lvl = ERROR;
  //     break;
  //   case CHIAKI_LOG_INFO:
  //     debugnet_lvl = INFO;
  //     break;
  //   // case CHIAKI_LOG_VERBOSE:
  //   case CHIAKI_LOG_DEBUG:
  //   // case CHIAKI_LOG_ALL:
  //     debugnet_lvl = DEBUG;
  //     break;
  //   default:
  //     return;
  // }
  if (lvl == CHIAKI_LOG_ALL || lvl == CHIAKI_LOG_VERBOSE) return;
  // uint64_t timestamp = sceKernelGetProcessTimeWide();
  sceClibPrintf("[CHIAKI] %s\n", msg);
  if (!context.stream.is_streaming) {
      if (context.mlog) {
        write_message_log(context.mlog, msg);
      }
    }
}

bool vita_chiaki_init_context() {
  config_parse(&context.config);

  // TODO: Load log level from config
  // TODO: Custom logging callback that logs to a file
  chiaki_log_init(&(context.log), CHIAKI_LOG_ALL & ~(CHIAKI_LOG_VERBOSE | CHIAKI_LOG_DEBUG), &log_cb_debugnet, NULL);
  context.mlog = message_log_create();

  write_message_log(context.mlog, "----- Debug log start -----"); // debug

  // init ui to select a certain button
  context.ui_state.active_item = UI_MAIN_WIDGET_MESSAGES_BTN;
  context.ui_state.next_active_item = context.ui_state.active_item;

  return true;
}
