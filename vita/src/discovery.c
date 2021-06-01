#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <chiaki/discoveryservice.h>
#include <chiaki/log.h>

#include <netinet/in.h>
#include <sys/socket.h>

#include "discovery.h"
#include "context.h"
#include "host.h"
#include "util.h"

/// Save a newly discovered host into the context
void save_discovered_host(ChiakiDiscoveryHost* host) {
  // Check if the host is already known, and if not, locate a free spot for it
  uint8_t host_mac[6];
  parse_hex(host->host_id, host_mac, sizeof(host_mac));
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
      if (strcmp(h->hostname, host->host_addr)) {
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
  memcpy(&(h->server_mac), &host_mac, 6);
  h->discovery_state =
      (ChiakiDiscoveryHost*)malloc(sizeof(ChiakiDiscoveryHost));
  memcpy(h->discovery_state, host, sizeof(ChiakiDiscoveryHost));
  h->hostname = (char*) h->discovery_state->host_addr;
  context.hosts[target_idx] = h;
  context.num_hosts++;

  // Check if the newly discovered host is a known registered one
  for (int rhost_idx = 0; rhost_idx < context.config.num_registered_hosts; rhost_idx++) {
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
  for (int dhost_idx = 0; dhost_idx < hosts_count; dhost_idx++) {
    save_discovered_host(&hosts[dhost_idx]);
  }

  // Call caller-defined callback
  VitaChiakiDiscoveryCallbackState* cb_state =
      (VitaChiakiDiscoveryCallbackState*)user;
  if (cb_state && cb_state->cb) {
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

  ChiakiErrorCode err = chiaki_discovery_service_init(&(context.discovery),
                                                      &opts, &(context.log));
  context.discovery_enabled = true;
  return err;
}

/// Terminate the Chiaki discovery thread, clean up discovey state in context
void stop_discovery() {
  if (!context.discovery_enabled) {
    return;
  }
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