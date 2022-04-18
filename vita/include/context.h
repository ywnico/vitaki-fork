#pragma once
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <chiaki/discoveryservice.h>
#include <chiaki/log.h>

#include "config.h"
#include "discovery.h"
#include "host.h"
#include "ui.h"
// #include "debugnet.h"

#define LOGD(fmt, ...) do {\
    uint64_t timestamp = sceKernelGetProcessTimeWide(); \
    sceClibPrintf("[DEBUG] %ju "fmt"\n", timestamp __VA_OPT__(,) __VA_ARGS__); \
  } while (0)
  //debugNetPrintf(DEBUG, "%ju "fmt"\n", timestamp __VA_OPT__(,) __VA_ARGS__);
#define LOGE(fmt, ...) do {\
    uint64_t timestamp = sceKernelGetProcessTimeWide(); \
    sceClibPrintf("[ERROR] %ju "fmt"\n", timestamp __VA_OPT__(,) __VA_ARGS__); \
  } while (0)
  //debugNetPrintf(ERROR, "%ju "fmt"\n", timestamp __VA_OPT__(,) __VA_ARGS__);

typedef struct vita_chiaki_stream_t {
  ChiakiSession session;
  ChiakiControllerState controller_state;
  bool session_init;
  bool is_streaming;
  int fps;
} VitaChiakiStream;

typedef struct vita_chiaki_context_t {
  ChiakiLog log;
  ChiakiDiscoveryService discovery;
  bool discovery_enabled;
  VitaChiakiDiscoveryCallbackState* discovery_cb_state;
  VitaChiakiHost* hosts[MAX_NUM_HOSTS];
  VitaChiakiHost* active_host;
  VitaChiakiStream stream;
  VitaChiakiConfig config;
  VitaChiakiUIState ui_state;
  uint8_t num_hosts;
} VitaChiakiContext;

/// Global context singleton
extern VitaChiakiContext context;

bool vita_chiaki_init_context();