#include "config.h"
#include "host.h"

void config_parse(char* filename, VitaChiakiConfig* cfg) {
  // TODO: Parse using inih
}

void config_free(VitaChiakiConfig* cfg) {
  if (cfg == NULL) {
    return;
  }
  free(cfg->psn_account_id);
  for (int i = 0; i < MAX_NUM_HOSTS; i++) {
    if (cfg->manual_hosts[i] != NULL) {
      free(cfg->manual_hosts[i]);
    }
    if (cfg->registered_hosts[i] != NULL) {
      free(cfg->registered_hosts[i]);
    }
  }
  free(cfg);
}

void config_serialize(char* filename, VitaChiakiConfig* cfg) {
  // TODO: Serialize using fprintf
}