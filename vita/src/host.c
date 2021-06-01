#include "context.h"
#include "host.h"

void host_free(VitaChiakiHost* host) {
  if (host->discovery_state) {
    free(host->discovery_state);
  }
  if (host->registered_state) {
    free(host->registered_state);
  }
  if (host->hostname) {
    free(host->hostname);
  }
}

int host_register(VitaChiakiHost* host, int pin) {
  if (!host->hostname || !host->discovery_state) {
    return 1;
  }
  // TODO: Register with host
  return 0;
}

int host_wakeup(VitaChiakiHost* host) {
  if (!host->hostname || !host->discovery_state) {
    return 1;
  }
  // TODO: Wake up host
  return 0;
}

/// Check if two MAC addresses match
bool mac_addrs_match(MacAddr* a, MacAddr* b) {
  for (int j = 0; j < 6; j++) {
    if (a[j] != b[j]) {
      return false;
    }
  }
  return true;
}