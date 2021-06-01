#include <psp2/kernel/processmgr.h>
#include <debugnet.h>

#include "context.h"

VitaChiakiContext context;

void log_cb_debugnet(ChiakiLogLevel lvl, const char *msg, void *user) {
  int debugnet_lvl;
  switch (lvl) {
    case CHIAKI_LOG_ERROR:
    case CHIAKI_LOG_WARNING:
      debugnet_lvl = ERROR;
      break;
    case CHIAKI_LOG_INFO:
      debugnet_lvl = INFO;
      break;
    //case CHIAKI_LOG_VERBOSE:
    case CHIAKI_LOG_DEBUG:
    //case CHIAKI_LOG_ALL:
      debugnet_lvl = DEBUG;
      break;
    default:
      return;
  }
  debugNetPrintf(debugnet_lvl, "[CHIAKI] %s\n", msg);
}

bool vita_chiaki_init_context() {
  // TODO: Only load default config if not present on disk
  context.config.psn_account_id = NULL;
  context.config.auto_discovery = true;
  context.config.disconnect_action = DISCONNECT_ACTION_ASK;
  context.config.resolution = CHIAKI_VIDEO_RESOLUTION_PRESET_540p;
  context.config.fps = CHIAKI_VIDEO_FPS_PRESET_60;

  // TODO: Load log level from config
  // TODO: Custom logging callback that logs to a file
  chiaki_log_init(&(context.log), CHIAKI_LOG_ALL, &log_cb_debugnet, NULL);
  return true;
}