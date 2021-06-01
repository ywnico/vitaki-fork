#include <sys/param.h>
#include <string.h>
#include <tomlc99/toml.h>

#include "config.h"
#include "context.h"
#include "host.h"
#include "util.h"

void zero_pad(char* buf, size_t size) {
  bool pad = false;
  for (int i=0; i < size; i++) {
    if (buf[i] == '\0') {
      pad = true;
    }
    if (pad) {
      buf[i] = '\0';
    }
  }
  if (!pad) {
    buf[size-1] = '\0';
  }
}

VitaChiakiDisconnectAction parse_disconnect_action(char* action) {
  if (strcmp(action, "ask") == 0) {
    return DISCONNECT_ACTION_ASK;
  } else if (strcmp(action, "rest") == 0) {
    return DISCONNECT_ACTION_REST;
  }
  return DISCONNECT_ACTION_NOTHING;
}

ChiakiVideoResolutionPreset parse_resolution_preset(char* preset) {
  if (strcmp(preset, "360p") == 0)
    return CHIAKI_VIDEO_RESOLUTION_PRESET_360p;
  return CHIAKI_VIDEO_RESOLUTION_PRESET_540p;
}

ChiakiTarget parse_target(char* target_name) {
  if (strcmp("ps4_unknown", target_name) == 0) {
    return CHIAKI_TARGET_PS4_UNKNOWN;
  } else if (strcmp("ps4_8", target_name) == 0) {
    return CHIAKI_TARGET_PS4_8;
  } else if (strcmp("ps4_9", target_name) == 0) {
    return CHIAKI_TARGET_PS4_9;
  } else if (strcmp("ps4_10", target_name) == 0) {
    return CHIAKI_TARGET_PS4_9;
  } else if (strcmp("ps5_unknown", target_name) == 0) {
    return CHIAKI_TARGET_PS5_UNKNOWN;
  } else if (strcmp("ps5_1", target_name) == 0) {
    return CHIAKI_TARGET_PS5_1;
  }
  return CHIAKI_TARGET_PS4_UNKNOWN;
}

VitaChiakiConfig* config_parse(char* filename) {
  VitaChiakiConfig* cfg = malloc(sizeof(VitaChiakiConfig));
  FILE* fp = fopen(filename, "w");
  char errbuf[200];
  toml_table_t* parsed = toml_parse_file(fp, errbuf, sizeof(errbuf));
  fclose(fp);
  if (!cfg) {
    CHIAKI_LOGE(&(context.log), "Failed to parse config due to illegal TOML: %s", errbuf);
    return NULL;
  }
  toml_table_t* general = toml_table_in(parsed, "general");
  if (!general) {
    CHIAKI_LOGE(&(context.log), "Failed to parse config due to missing [general] section");
    return NULL;
  }
  toml_datum_t datum;
  datum = toml_int_in(general, "version");
  if (!datum.ok || datum.u.i != CFG_VERSION) {
    CHIAKI_LOGE(&(context.log), "Failed to parse config due to bad general.version, expected %d.", CFG_VERSION);
    return NULL;
  }

  toml_table_t* settings = toml_table_in(parsed, "settings");
  if (settings) {
    datum = toml_bool_in(settings, "auto_discovery");
    cfg->auto_discovery = datum.ok ? datum.u.b : true;
    datum = toml_string_in(settings, "disconnect_action");
    if (datum.ok) {
      cfg->disconnect_action = parse_disconnect_action(datum.u.s);
      free(datum.u.s);
    } else {
      cfg->disconnect_action = DISCONNECT_ACTION_ASK;
    }
    datum = toml_string_in(settings, "resolution");
    if (datum.ok) {
      cfg->resolution = parse_resolution_preset(datum.u.s);
      free(datum.u.s);
    } else {
      cfg->resolution = CHIAKI_VIDEO_RESOLUTION_PRESET_540p;
    }
    datum = toml_int_in(settings, "fps");
    if (datum.ok) {
      cfg->fps = datum.u.i == 30 ? CHIAKI_VIDEO_FPS_PRESET_30 : CHIAKI_VIDEO_FPS_PRESET_60;
    } else {
      cfg->fps = CHIAKI_VIDEO_FPS_PRESET_60;
    }
    datum = toml_string_in(settings, "psn_account_id");
    if (datum.ok) {
      cfg->psn_account_id = datum.u.s;
    }
  }

  toml_array_t* regist_hosts = toml_array_in(parsed, "registered_hosts");
  if (regist_hosts && toml_array_kind(regist_hosts) == 't') {
    int num_rhosts = toml_array_nelem(regist_hosts);
    for (int i=0; i < MIN(MAX_NUM_HOSTS, num_rhosts) ; i++) {
      VitaChiakiHost* host = malloc(sizeof(VitaChiakiHost));
      ChiakiRegisteredHost* rstate = malloc(sizeof(ChiakiRegisteredHost));
      host->registered_state = rstate;
      toml_table_t* host_cfg = toml_table_at(regist_hosts, i);
      datum = toml_string_in(host_cfg, "server_mac");
      if (datum.ok) {
        parse_hex(datum.u.s, host->server_mac, 6);
        free(datum.u.s);
      }
      datum = toml_string_in(host_cfg, "server_nickname");
      if (datum.ok) {
        strncpy(rstate->server_nickname, datum.u.s, sizeof(rstate->server_nickname));
        rstate->server_nickname[sizeof(rstate->server_nickname)-1] = '\0';
        free(datum.u.s);
      }
      datum = toml_string_in(host_cfg, "target");
      if (datum.ok) {
        rstate->target = parse_target(datum.u.s);
        free(datum.u.s);
      }
      datum = toml_string_in(host_cfg, "rp_key");
      if (datum.ok) {
        parse_hex(datum.u.s, rstate->rp_key, 0x10);
        free(datum.u.s);
      }
      datum = toml_int_in(host_cfg, "rp_key_type");
      if (datum.ok) {
        rstate->rp_key_type = datum.u.i;
      }
      datum = toml_string_in(host_cfg, "rp_regist_key");
      if (datum.ok) {
        strncpy(rstate->rp_regist_key, datum.u.s, sizeof(rstate->rp_regist_key));
        zero_pad(rstate->rp_regist_key, sizeof(rstate->rp_regist_key));
        free(datum.u.s);
      }
      datum = toml_string_in(host_cfg, "ap_bssid");
      if (datum.ok) {
        strncpy(rstate->ap_bssid, datum.u.s, sizeof(rstate->ap_bssid));
        zero_pad(rstate->ap_bssid, sizeof(rstate->ap_bssid));
        free(datum.u.s);
      }
      datum = toml_string_in(host_cfg, "ap_key");
      if (datum.ok) {
        strncpy(rstate->ap_key, datum.u.s, sizeof(rstate->ap_key));
        zero_pad(rstate->ap_bssid, sizeof(rstate->ap_bssid));
        free(datum.u.s);
      }
      datum = toml_string_in(host_cfg, "ap_ssid");
      if (datum.ok) {
        strncpy(rstate->ap_ssid, datum.u.s, sizeof(rstate->ap_ssid));
        zero_pad(rstate->ap_ssid, sizeof(rstate->ap_ssid));
        free(datum.u.s);
      }
      datum = toml_string_in(host_cfg, "ap_name");
      if (datum.ok) {
        strncpy(rstate->ap_name, datum.u.s, sizeof(rstate->ap_name));
        zero_pad(rstate->ap_name, sizeof(rstate->ap_name));
        free(datum.u.s);
      }
      cfg->registered_hosts[i] = host;
      cfg->num_registered_hosts++;
    }
  }

  toml_array_t* manual_hosts = toml_array_in(parsed, "manual_hosts");
  if (manual_hosts && toml_array_kind(manual_hosts) == 't') {
    int num_mhosts = toml_array_nelem(manual_hosts);
    for (int i=0; i < MIN(MAX_NUM_HOSTS, num_mhosts) ; i++) {
      VitaChiakiHost* host;
      toml_table_t* host_cfg = toml_table_at(manual_hosts, i);
      datum = toml_string_in(host_cfg, "server_mac");
      uint8_t server_mac[6];
      if (datum.ok) {
        // We have a MAC for the manual host, try to find corresponding
        // registered host
        parse_hex(datum.u.s, server_mac, sizeof(server_mac));
        free(datum.u.s);
        for (int hidx=0; hidx < cfg->num_registered_hosts; hidx++) {
          uint8_t* candidate_mac = cfg->registered_hosts[hidx]->server_mac;
          for (int j=0; j < sizeof(server_mac); j++) {
            if (candidate_mac[j] == server_mac[j]) {
              host = cfg->registered_hosts[hidx];
              break;
            }
          }
        }
      }
      if (!host) {
        // No corresponding registered host found, create new host and assign
        // MAC if specified.
        host = malloc(sizeof(VitaChiakiHost));
        if (datum.ok) {
          memcpy(host->server_mac, server_mac, sizeof(server_mac));
        }
      }

      datum = toml_string_in(host_cfg, "hostname");
      if (datum.ok) {
        host->hostname = datum.u.s;
        cfg->manual_hosts[i] = host;
        cfg->num_manual_hosts++;
      } else {
        CHIAKI_LOGW(&(context.log), "Failed to parse manual host due to missing hostname.");
        free(host);
      }
    }
  }

  return cfg;
}

void config_free(VitaChiakiConfig* cfg) {
  if (cfg == NULL) {
    return;
  }
  free(cfg->psn_account_id);
  for (int i = 0; i < MAX_NUM_HOSTS; i++) {
    if (cfg->manual_hosts[i] != NULL) {
      host_free(cfg->manual_hosts[i]);
    }
    if (cfg->registered_hosts[i] != NULL) {
      host_free(cfg->registered_hosts[i]);
    }
  }
  free(cfg);
}

char* serialize_disconnect_action(VitaChiakiDisconnectAction action) {
  switch (action) {
    case DISCONNECT_ACTION_ASK:
      return "ask";
    case DISCONNECT_ACTION_REST:
      return "rest";
    case DISCONNECT_ACTION_NOTHING:
      return "nothing";
  }
}

char* serialize_resolution_preset(ChiakiVideoResolutionPreset preset) {
  switch (preset) {
    case CHIAKI_VIDEO_RESOLUTION_PRESET_360p:
      return "360p";
    default:
      // We don't support solutions higher than 540p on the Vita
      // TODO: Should we? For PSTV?
      return "540p";
  }
}

void serialize_hex(FILE* fp, char* field_name, uint8_t* val, size_t len) {
  bool all_zero = true;
  for (size_t i=0; i < len; i++) {
    if (val[i] != 0) {
      all_zero = false;
      break;
    }
  }
  if (all_zero) {
    return;
  }
  fprintf(fp, "%s = \"", field_name);
  for (size_t i=0; i < len; i++) {
    fprintf(fp, "%02X", val[i]);
  }
  fprintf(fp, "\"\n");
}

void serialize_target(FILE* fp, char* field_name, ChiakiTarget* target) {
  fprintf(fp, "%s = \"", field_name);
  switch (*target) {
    case CHIAKI_TARGET_PS4_UNKNOWN:
      fprintf(fp, "ps4_unknown");
      break;
    case CHIAKI_TARGET_PS4_8:
      fprintf(fp, "ps4_8");
      break;
    case CHIAKI_TARGET_PS4_9:
      fprintf(fp, "ps4_9");
      break;
    case CHIAKI_TARGET_PS4_10:
      fprintf(fp, "ps4_10");
      break;
    case CHIAKI_TARGET_PS5_UNKNOWN:
      fprintf(fp, "ps5_unknown");
      break;
    case CHIAKI_TARGET_PS5_1:
      fprintf(fp, "ps5_1");
      break;
  }
  fprintf(fp, "\"\n");
}

void config_serialize(char* filename, VitaChiakiConfig* cfg) {
  FILE *fp = fopen(filename, "w");
  fprintf(fp, "[general]\nversion = 1\n");

  // Settings
  fprintf(fp, "[settings]\n");
  fprintf(fp, "auto_discovery = %s\n",
          cfg->auto_discovery ? "true" : "false");
  fprintf(fp, "disconnect_action = \"%s\"\n",
          serialize_disconnect_action(cfg->disconnect_action));
  fprintf(fp, "resolution = \"%s\"\n",
          serialize_resolution_preset(cfg->resolution));
  fprintf(fp, "fps = %d\n", cfg->fps);
  if (cfg->psn_account_id) {
    fprintf(fp, "psn_account_id = \"%s\"\n", cfg->psn_account_id);
  }

  for (int i = 0; i < cfg->num_manual_hosts; i++) {
    VitaChiakiHost* host = cfg->manual_hosts[i];
    fprintf(fp, "\n\n[[manual_hosts]]\n");
    fprintf(fp, "hostname = \"%s\"\n", host->hostname);
    uint8_t* mac = NULL;
    for (int m = 0; m < 6; m++) {
      if (host->server_mac[m] != 0) {
        mac = host->server_mac;
        break;
      }
    }
    if (mac) {
      serialize_hex(fp, "server_mac", mac, 6);
    }
  }

  for (int i = 0; i < cfg->num_registered_hosts; i++) {
    ChiakiRegisteredHost* rhost = cfg->manual_hosts[i]->registered_state;
    fprintf(fp, "\n\n[[registered_hosts]]\n");
    serialize_hex(fp, "server_mac", rhost->server_mac, 6);
    fprintf(fp, "server_nickname = \"%s\"\n", rhost->server_nickname);
    serialize_target(fp, "target", &rhost->target);
    serialize_hex(fp, "rp_key", &(rhost->rp_key[0]), 0x10);
    fprintf(fp, "rp_key_type = %d\n", rhost->rp_key_type);
    fprintf(fp, "rp_regist_key = \"%s\"\n", rhost->rp_regist_key);
    fprintf(fp, "ap_bssid = \"%s\"\n", rhost->ap_bssid);
    fprintf(fp, "ap_key = \"%s\"\n", rhost->ap_key);
    fprintf(fp, "ap_ssid = \"%s\"\n", rhost->ap_ssid);
    fprintf(fp, "ap_name = \"%s\"\n", rhost->ap_name);
  }
  fclose(fp);
}