#pragma once
#include <chiaki/discoveryservice.h>
#include <chiaki/log.h>

#include "config.h"
#include "discovery.h"
#include "host.h"
#include "ui.h"

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