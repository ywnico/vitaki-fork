#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <chiaki/discoveryservice.h>
#include <chiaki/log.h>
#include <debugnet.h>

#include <netinet/in.h>
#include <sys/socket.h>

#include "discovery.h"
#include "context.h"
#include "host.h"

typedef uint8_t MacAddr[6];

/// Check if two MAC addresses match
bool mac_addrs_match(MacAddr* a, MacAddr* b) {
  for (int j = 0; j < 6; j++) {
    if (a[j] != b[j]) {
      return false;
    }
  }
  return true;
}

/// Save a newly discovered host into the context
void save_discovered_host(ChiakiDiscoveryHost* host) {
  uint8_t host_mac[6];
  // Check if the host is already known, and if not, locate a free spot for it
  sscanf(host->host_id, "%hhx%hhx%hhx%hhx%hhx%hhx", &(host_mac[0]),
         &(host_mac[1]), &(host_mac[2]), &(host_mac[3]), &(host_mac[4]),
         &(host_mac[5]));
  int target_idx = -1;
  VitaChiakiHost* h;
  for (int host_idx = 0; host_idx < MAX_NUM_HOSTS; host_idx++) {
    h = context.hosts[host_idx];
    if (h == NULL) {
      // Found a free spot
      target_idx = host_idx;
    } else if (h->type & DISCOVERED) {
      if (mac_addrs_match(&(h->server_mac), &host_mac)) {
        // Already known discovered hosts, we can skip saving
        return;
      }
    } else if (h->type & MANUALLY_ADDED) {
      if (strcmp(h->host, host->host_addr)) {
        // Manually added host matched this discovered host, update
        target_idx = host_idx;
        break;
      }
    }
  }

  // Maximum number of hosts reached, can't save host
  // TODO: Indicate to user?
  if (target_idx < 0) {
    return;
  }

  if (!h) {
    h = (VitaChiakiHost*)malloc(sizeof(VitaChiakiHost));
  }

  h->type |= DISCOVERED;
  h->target = chiaki_discovery_host_system_version_target(host);
  h->discovery_state =
      (ChiakiDiscoveryHost*)malloc(sizeof(ChiakiDiscoveryHost));
  memcpy(&(h->server_mac), &host_mac, 6);
  strcpy(h->host, h->discovery_state->host_addr);
  memcpy(h->discovery_state, host, sizeof(ChiakiDiscoveryHost));
  context.hosts[target_idx] = h;
  context.num_hosts++;

  // Check if the newly discovered host is a known registered one
  for (int rhost_idx = 0; rhost_idx < MAX_NUM_HOSTS; rhost_idx++) {
    ChiakiRegisteredHost* rhost =
        context.config.registered_hosts[rhost_idx]->registered_state;
    if (rhost == NULL) {
      continue;
    }
    if (mac_addrs_match(&(rhost->server_mac), &(h->server_mac))) {
      h->registered_state = rhost;
      break;
    }
  }
}

/// Called whenever new hosts are discovered
void discovery_cb(ChiakiDiscoveryHost* hosts, size_t hosts_count, void* user) {
  debugNetPrintf(DEBUG, "Got discovery results, found %d hosts.\n", hosts_count);
  for (int dhost_idx = 0; dhost_idx < hosts_count; dhost_idx++) {
    save_discovered_host(hosts + dhost_idx);
  }

  // Call caller-defined callback
  VitaChiakiDiscoveryCallbackState* cb_state =
      (VitaChiakiDiscoveryCallbackState*)user;
  if (cb_state != NULL && cb_state->cb != NULL) {
    cb_state->cb(cb_state->cb_user);
  }
}

/// Initiate the Chiaki discovery thread
ChiakiErrorCode start_discovery(VitaChiakiDiscoveryCb cb, void* cb_user) {
  if (context.discovery_enabled) {
    return CHIAKI_ERR_SUCCESS;
  }
  if (cb != NULL) {
    context.discovery_cb_state =
        malloc(sizeof(VitaChiakiDiscoveryCallbackState));
    context.discovery_cb_state->cb = cb;
    context.discovery_cb_state->cb_user = cb_user;
  }
  debugNetPrintf(DEBUG, "Setting up discovery options\n");
  ChiakiDiscoveryServiceOptions opts;
  opts.cb = discovery_cb;
  opts.cb_user = context.discovery_cb_state;
  opts.ping_ms = 500;
  opts.hosts_max = MAX_NUM_HOSTS;

  sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = SCE_NET_INADDR_BROADCAST;
  opts.send_addr = (struct sockaddr*) &addr;
  opts.send_addr_size = sizeof(addr);

  debugNetPrintf(DEBUG, "Initializing chiaki discovery service\n");
  ChiakiErrorCode err = chiaki_discovery_service_init(&(context.discovery),
                                                      &opts, &(context.log));
  debugNetPrintf(DEBUG, "Result of discovery service initialization: %d\n", err);
  context.discovery_enabled = true;
  return err;
}

/// Terminate the Chiaki discovery thread, clean up discovey state in context
void stop_discovery() {
  if (!context.discovery_enabled) {
    return;
  }
  debugNetPrintf(DEBUG, "Stopping chiaki discovery service.\n");
  chiaki_discovery_service_fini(&(context.discovery));
  context.discovery_enabled = false;
  for (int i = 0; i < MAX_NUM_HOSTS; i++) {
    VitaChiakiHost* h = context.hosts[i];
    if (h == NULL) {
      continue;
    }
    if (!(h->type & MANUALLY_ADDED)) {
      // Discovered host, can be completely freed
      free(h->discovery_state);
      if (h->registered_state) {
        free(h->registered_state);
      }
      context.hosts[i] = NULL;
      context.num_hosts--;
    } else if (h->type & DISCOVERED) {
      // Discovered host that's also manually added, only free the discovery
      // state
      free(h->discovery_state);
    }
  }
  if (context.discovery_cb_state != NULL) {
    free(context.discovery_cb_state);
    context.discovery_cb_state = NULL;
  }
}