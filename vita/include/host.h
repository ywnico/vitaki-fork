#pragma once
#include <chiaki/common.h>
#include <chiaki/discovery.h>
#include <chiaki/regist.h>

#define MAX_NUM_HOSTS 4

typedef enum vita_chiaki_host_type_t {
  DISCOVERED = 0x01,
  MANUALLY_ADDED = 0x02,
  REGISTERED = 0x04,
} VitaChiakiHostType;

typedef struct vita_chiaki_host_t {
  VitaChiakiHostType type;
  ChiakiTarget target;
  uint8_t server_mac[6];
  char host[0x20];

  ChiakiDiscoveryHost* discovery_state;
  ChiakiRegisteredHost* registered_state;
} VitaChiakiHost;

void host_free(VitaChiakiHost* host);
int host_register(VitaChiakiHost* host, int pin);
int host_wakeup(VitaChiakiHost* host);