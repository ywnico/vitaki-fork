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
  char* hostname;

  ChiakiDiscoveryHost* discovery_state;
  ChiakiRegisteredHost* registered_state;
} VitaChiakiHost;


typedef uint8_t MacAddr[6];

void host_free(VitaChiakiHost* host);
int host_register(VitaChiakiHost* host, int pin);
int host_wakeup(VitaChiakiHost* host);
bool mac_addrs_match(MacAddr* a, MacAddr* b);
