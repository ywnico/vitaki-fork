#pragma once
#include <psp2/kernel/processmgr.h>
#include <chiaki/discoveryservice.h>
#include <chiaki/log.h>

#include "config.h"
#include "discovery.h"
#include "host.h"
#include "ui.h"
#include "debugnet.h"

#define LOGD(fmt, ...) do {\
    uint64_t timestamp = sceKernelGetProcessTimeWide(); \
    debugNetPrintf(DEBUG, "%ju "fmt"\n", timestamp __VA_OPT__(,) __VA_ARGS__); \
  } while (0)
#define LOGE(fmt, ...) do {\
    uint64_t timestamp = sceKernelGetProcessTimeWide(); \
    debugNetPrintf(ERROR, "%ju "fmt"\n", timestamp __VA_OPT__(,) __VA_ARGS__); \
  } while (0)

typedef struct vita_chiaki_context_t {
  ChiakiLog log;
  ChiakiDiscoveryService discovery;
  bool discovery_enabled;
  VitaChiakiDiscoveryCallbackState* discovery_cb_state;
  VitaChiakiHost* hosts[MAX_NUM_HOSTS];
  VitaChiakiConfig config;
  VitaChiakiUIState ui_state;
  uint8_t num_hosts;
} VitaChiakiContext;

/// Global context singleton
extern VitaChiakiContext context;

bool vita_chiaki_init_context();