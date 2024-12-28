#include "context.h"
#include "config.h"
#include "controller.h"
#include "host.h"
#include "discovery.h"
#include "audio.h"
#include "video.h"
#include "string.h"
#include <stdio.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <chiaki/base64.h>
#include <chiaki/session.h>

void host_init(VitaChiakiHost* host) {
}

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
ChiakiRegist regist = {};
static void regist_cb(ChiakiRegistEvent *event, void *user) {
  LOGD("regist event %d", event->type);
  if (event->type == CHIAKI_REGIST_EVENT_TYPE_FINISHED_SUCCESS) {
    context.active_host->type |= REGISTERED;

    if (context.active_host->registered_state != NULL) {
      free(context.active_host->registered_state);
      context.active_host->registered_state = event->registered_host;
      printf("FOUND HOST TO UPDATE\n");
      for (int rhost_idx = 0; rhost_idx < context.config.num_registered_hosts; rhost_idx++) {
        VitaChiakiHost* rhost =
            context.config.registered_hosts[rhost_idx];
        if (rhost == NULL) {
          continue;
        }

        // FIXME: Use MAC instead of name
        printf("NAME1 %s\n", rhost->registered_state->server_nickname);
        printf("NAME2 %s\n", context.active_host->registered_state->server_nickname);
        if (strcmp(rhost->registered_state->server_nickname, context.active_host->registered_state->server_nickname) == 0) {
          printf("FOUND MATCH\n");
          context.config.registered_hosts[rhost_idx] = context.active_host;
          break;
        }
      }
    } else {
      context.active_host->registered_state = event->registered_host;
      context.config.registered_hosts[context.config.num_registered_hosts++] = context.active_host;
    }

    config_serialize(&context.config);
  }

  chiaki_regist_stop(&regist);
	chiaki_regist_fini(&regist);
}

int host_register(VitaChiakiHost* host, int pin) {
  if (!host->hostname || !host->discovery_state) {
    return 1;
  }
  ChiakiRegistInfo regist_info = {};
  regist_info.target = host->target;
  size_t account_id_size = sizeof(uint8_t[CHIAKI_PSN_ACCOUNT_ID_SIZE]);
  chiaki_base64_decode(context.config.psn_account_id, /*sizeof(context.config.psn_account_id)*/12, regist_info.psn_account_id, &(account_id_size));
  regist_info.psn_online_id = NULL;
	regist_info.pin = pin;
  regist_info.host = host->hostname;
  regist_info.broadcast = false;
	chiaki_regist_start(&regist, &context.log, &regist_info, regist_cb, NULL);
  return 0;
}

int host_wakeup(VitaChiakiHost* host) {
  if (!host->hostname || !host->discovery_state) {
    return 1;
  }
  chiaki_discovery_wakeup(&context.log, context.discovery_enabled ? &context.discovery.discovery : NULL, host->hostname, *host->registered_state->rp_regist_key, chiaki_target_is_ps5(host->target));
  return 0;
}

static void event_cb(ChiakiEvent *event, void *user) {
	switch(event->type)
	{
		case CHIAKI_EVENT_CONNECTED:
			LOGD("EventCB CHIAKI_EVENT_CONNECTED");
			break;
		case CHIAKI_EVENT_LOGIN_PIN_REQUEST:
			LOGD("EventCB CHIAKI_EVENT_LOGIN_PIN_REQUEST");
			break;
		case CHIAKI_EVENT_RUMBLE:
			LOGD("EventCB CHIAKI_EVENT_RUMBLE");
			break;
		case CHIAKI_EVENT_QUIT:
			LOGE("EventCB CHIAKI_EVENT_QUIT");
	    chiaki_opus_decoder_fini(&context.stream.opus_decoder);
      vita_h264_cleanup();
      vita_audio_cleanup();
      context.stream.is_streaming = false;
			break;
	}
}

static bool video_cb(uint8_t *buf, size_t buf_size, void *user) {
  context.stream.is_streaming = true;
  int err = vita_h264_decode_frame(buf, buf_size);
  if (err != 0) {
		LOGE("Error during video decode: %d", err);
    return false;
  }
  return true;
}

static void set_ctrl_out(VitaChiakiStream *stream, VitakiCtrlOut ctrl_out) {
  if (ctrl_out == VITAKI_CTRL_OUT_L2) {
    stream->controller_state.l2_state = 0xff;
  } else if (ctrl_out == VITAKI_CTRL_OUT_R2) {
    stream->controller_state.r2_state = 0xff;
  } else {
    // in this case ctrl_out is a controller button mask
    //if (ctrl_out == VITAKI_CTRL_OUT_NONE) {
    //// do nothing
    //} else {
    stream->controller_state.buttons |= ctrl_out;
    //}
  }
}

void set_ctrl_l2pos(VitaChiakiStream *stream, VitakiCtrlIn ctrl_in) {
  // check if ctrl_in should be l2; if not, hit corresponding mapped button
  VitakiCtrlMapInfo vcmi = stream->vcmi;
  if (vcmi.in_l2 == ctrl_in) {
    stream->controller_state.l2_state = 0xff;
  } else {
    stream->controller_state.buttons |= vcmi.in_out_btn[ctrl_in];
  }
}
void set_ctrl_r2pos(VitaChiakiStream *stream, VitakiCtrlIn ctrl_in) {
  // check if ctrl_in should be r2; if not, hit corresponding mapped button
  VitakiCtrlMapInfo vcmi = stream->vcmi;
  if (vcmi.in_r2 == ctrl_in) {
    stream->controller_state.r2_state = 0xff;
  } else {
    stream->controller_state.buttons |= vcmi.in_out_btn[ctrl_in];
  }
}

static void *input_thread_func(void* user) {
  sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
  sceCtrlSetSamplingModeExt(SCE_CTRL_MODE_ANALOG_WIDE);
  SceCtrlData ctrl;
	VitaChiakiStream *stream = user;
  int ms_per_loop = 5;

  VitakiCtrlMapInfo vcmi = stream->vcmi;

  if (!vcmi.did_init) init_controller_map(&vcmi, context.config.controller_map_id);

  // Touchscreen setup
	sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
	sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);
	sceTouchEnableTouchForce(SCE_TOUCH_PORT_FRONT);
	sceTouchEnableTouchForce(SCE_TOUCH_PORT_BACK);
	SceTouchData touch[SCE_TOUCH_PORT_MAX_NUM];
  int TOUCH_MAX_WIDTH = 1919;
  int TOUCH_MAX_HEIGHT = 1087;
  int TOUCH_MAX_WIDTH_BY_2 = TOUCH_MAX_WIDTH/2;
  int TOUCH_MAX_HEIGHT_BY_2 = TOUCH_MAX_HEIGHT/2;
  int TOUCH_MAX_WIDTH_BY_4 = TOUCH_MAX_WIDTH/4;
  int TOUCH_MAX_HEIGHT_BY_4 = TOUCH_MAX_HEIGHT/4;
  int FRONT_ARC_RADIUS = TOUCH_MAX_HEIGHT/3;
  int FRONT_ARC_RADIUS_2 = FRONT_ARC_RADIUS*FRONT_ARC_RADIUS;
  // note: rear touch may be active in Y only from 108 to 889? (see Vita3K code)

  // manually create bools for each combo (since there's only 5)
  bool vitaki_reartouch_left_l1_mapped = (vcmi.in_out_btn[VITAKI_CTRL_IN_REARTOUCH_LEFT_L1] != 0) || (vcmi.in_l2 == VITAKI_CTRL_IN_REARTOUCH_LEFT_L1);
  bool vitaki_reartouch_right_r1_mapped = (vcmi.in_out_btn[VITAKI_CTRL_IN_REARTOUCH_RIGHT_R1] != 0) || (vcmi.in_r2 == VITAKI_CTRL_IN_REARTOUCH_RIGHT_R1);
  bool vitaki_select_start_mapped = (vcmi.in_out_btn[VITAKI_CTRL_IN_SELECT_START] != 0);
  bool vitaki_left_square_mapped = (vcmi.in_out_btn[VITAKI_CTRL_IN_LEFT_SQUARE] != 0) || (vcmi.in_l2 == VITAKI_CTRL_IN_LEFT_SQUARE);
  bool vitaki_right_circle_mapped = (vcmi.in_out_btn[VITAKI_CTRL_IN_RIGHT_CIRCLE] != 0) || (vcmi.in_r2 == VITAKI_CTRL_IN_RIGHT_CIRCLE);

  while (true) {

    // TODO enable using triggers as L2, R2
    // TODO enable home button, with long hold sent back to Vita?


    if (stream->is_streaming) {
      int start_time_us = sceKernelGetProcessTimeWide();

      // get button state
      sceCtrlPeekBufferPositiveExt2(0, &ctrl, 1);

      // get touchscreen state
      for(int port = 0; port < SCE_TOUCH_PORT_MAX_NUM; port++) {
        sceTouchPeek(port, &touch[port], 1);
      }

      // 0-255 conversion
      stream->controller_state.left_x = (ctrl.lx - 128) * 2 * 0x7F/*.FF*/;
      stream->controller_state.left_y = (ctrl.ly - 128) * 2 * 0x7F/*.FF*/;
      stream->controller_state.right_x = (ctrl.rx - 128) * 2 * 0x7F/*.FF*/;
      stream->controller_state.right_y = (ctrl.ry - 128) * 2 * 0x7F/*.FF*/;

      stream->controller_state.buttons = 0x00;
      stream->controller_state.l2_state = 0x00;
      stream->controller_state.r2_state = 0x00;

      bool reartouch_right = false;
      bool reartouch_left = false;

      for (int touch_i = 0; touch_i < touch[SCE_TOUCH_PORT_BACK].reportNum; touch_i++) {
        int x = touch[SCE_TOUCH_PORT_BACK].report[touch_i].x;
        int y = touch[SCE_TOUCH_PORT_BACK].report[touch_i].y;

        stream->controller_state.buttons |= vcmi.in_out_btn[VITAKI_CTRL_IN_REARTOUCH_ANY];

        if (x > TOUCH_MAX_WIDTH_BY_2) {
          set_ctrl_r2pos(stream, VITAKI_CTRL_IN_REARTOUCH_RIGHT);
          reartouch_right = true;
          if (y > TOUCH_MAX_HEIGHT_BY_2) {
            set_ctrl_r2pos(stream, VITAKI_CTRL_IN_REARTOUCH_LR);
          } else {
            set_ctrl_r2pos(stream, VITAKI_CTRL_IN_REARTOUCH_UR);
          }
        } else if (x < TOUCH_MAX_WIDTH_BY_2) {
          set_ctrl_l2pos(stream, VITAKI_CTRL_IN_REARTOUCH_LEFT);
          reartouch_left = true;
          if (y > TOUCH_MAX_HEIGHT_BY_2) {
            set_ctrl_l2pos(stream, VITAKI_CTRL_IN_REARTOUCH_LL);
          } else {
            set_ctrl_l2pos(stream, VITAKI_CTRL_IN_REARTOUCH_UL);
          }
        }
      }

      for (int touch_i = 0; touch_i < touch[SCE_TOUCH_PORT_FRONT].reportNum; touch_i++) {
        int x = touch[SCE_TOUCH_PORT_FRONT].report[touch_i].x;
        int y = touch[SCE_TOUCH_PORT_FRONT].report[touch_i].y;
        stream->controller_state.buttons |= vcmi.in_out_btn[VITAKI_CTRL_IN_FRONTTOUCH_ANY];

        if (x > TOUCH_MAX_WIDTH_BY_2) {
          set_ctrl_r2pos(stream, VITAKI_CTRL_IN_FRONTTOUCH_RIGHT);

          if (y*y + (x-TOUCH_MAX_WIDTH)*(x-TOUCH_MAX_WIDTH) <= FRONT_ARC_RADIUS_2) {
            set_ctrl_r2pos(stream, VITAKI_CTRL_IN_FRONTTOUCH_UR_ARC);
          } else if ((y-TOUCH_MAX_HEIGHT)*(y-TOUCH_MAX_HEIGHT) + (x-TOUCH_MAX_WIDTH)*(x-TOUCH_MAX_WIDTH) <= FRONT_ARC_RADIUS_2) {
            set_ctrl_r2pos(stream, VITAKI_CTRL_IN_FRONTTOUCH_LR_ARC);
          }
        } else if (x < TOUCH_MAX_WIDTH_BY_2) {
          set_ctrl_l2pos(stream, VITAKI_CTRL_IN_FRONTTOUCH_LEFT);

          if (y*y + x*x <= FRONT_ARC_RADIUS_2) {
            set_ctrl_l2pos(stream, VITAKI_CTRL_IN_FRONTTOUCH_UL_ARC);
          } else if ((y-TOUCH_MAX_HEIGHT)*(y-TOUCH_MAX_HEIGHT) + x*x <= FRONT_ARC_RADIUS_2) {
            set_ctrl_l2pos(stream, VITAKI_CTRL_IN_FRONTTOUCH_LL_ARC);
          }
        }

        if ((x >= TOUCH_MAX_WIDTH_BY_4) && (x <= TOUCH_MAX_WIDTH - TOUCH_MAX_WIDTH_BY_4)
            && (y >= TOUCH_MAX_HEIGHT_BY_4) && (y <= TOUCH_MAX_HEIGHT - TOUCH_MAX_HEIGHT_BY_4)
            ) {
          stream->controller_state.buttons |= vcmi.in_out_btn[VITAKI_CTRL_IN_FRONTTOUCH_CENTER];
        }
      }

      // cursed conversion
      if (ctrl.buttons & SCE_CTRL_SELECT)   stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_SHARE;
      if (ctrl.buttons & SCE_CTRL_START)    stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_OPTIONS;
      if (ctrl.buttons & SCE_CTRL_UP)       stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_UP;
      if (ctrl.buttons & SCE_CTRL_RIGHT)    stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT;
      if (ctrl.buttons & SCE_CTRL_DOWN)     stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN;
      if (ctrl.buttons & SCE_CTRL_LEFT)     stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT;
      if (ctrl.buttons & SCE_CTRL_TRIANGLE) stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_PYRAMID;
      if (ctrl.buttons & SCE_CTRL_CIRCLE)   stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_MOON;
      if (ctrl.buttons & SCE_CTRL_CROSS)    stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_CROSS;
      if (ctrl.buttons & SCE_CTRL_SQUARE)   stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_BOX;
      // what is L3??
      if (ctrl.buttons & SCE_CTRL_L3)       stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_L3;
      if (ctrl.buttons & SCE_CTRL_R3)       stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_R3;

      if (ctrl.buttons & SCE_CTRL_L1) {
        if (reartouch_left && vitaki_reartouch_left_l1_mapped) {
          set_ctrl_l2pos(stream, VITAKI_CTRL_IN_REARTOUCH_LEFT_L1);
        } else {
          set_ctrl_l2pos(stream, VITAKI_CTRL_IN_L1);
        }
      }
      if (ctrl.buttons & SCE_CTRL_R1) {
        if (reartouch_right && vitaki_reartouch_right_r1_mapped) {
          set_ctrl_r2pos(stream, VITAKI_CTRL_IN_REARTOUCH_RIGHT_R1);
        } else {
          set_ctrl_r2pos(stream, VITAKI_CTRL_IN_R1);
        }
      }

      // Select + Start
      if (vitaki_select_start_mapped) {
        if ((ctrl.buttons & SCE_CTRL_SELECT) && (ctrl.buttons & SCE_CTRL_START)) {
          stream->controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_SHARE;
          stream->controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_OPTIONS;
          stream->controller_state.buttons |= vcmi.in_out_btn[VITAKI_CTRL_IN_SELECT_START];
        }
      }

      // Dpad-left + Square
      if (vitaki_left_square_mapped) {
        if ((ctrl.buttons & SCE_CTRL_LEFT) && (ctrl.buttons & SCE_CTRL_SQUARE)) {
          stream->controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT;
          stream->controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_BOX;
          set_ctrl_l2pos(stream, VITAKI_CTRL_IN_LEFT_SQUARE);
        }
      }
      // Dpad-right + Circle
      if (vitaki_right_circle_mapped) {
        if ((ctrl.buttons & SCE_CTRL_RIGHT) && (ctrl.buttons & SCE_CTRL_CIRCLE)) {
          stream->controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT;
          stream->controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_MOON;
          set_ctrl_r2pos(stream, VITAKI_CTRL_IN_RIGHT_CIRCLE);
        }
      }

      chiaki_session_set_controller_state(&stream->session, &stream->controller_state);
      // LOGD("ly 0x%x %d", ctrl.ly, ctrl.ly);

      // Adjust sleep time to account for calculations above
      int diff_time_us = sceKernelGetProcessTimeWide() - start_time_us;
      if (diff_time_us >= ms_per_loop*1000) {
        // Control loop appears to usually take ~70-90 us, sometimes up to 130
        // Far less than the 5000 us of the default loop
        LOGD("SLOW CTRL LOOP! %d microseconds", diff_time_us);
      } else {
        usleep(ms_per_loop*1000 - diff_time_us);
      }

    }
  }

  return 0;
}

int host_stream(VitaChiakiHost* host) {
  LOGD("Preparing to start host_stream");
  if (!host->hostname || !host->registered_state) {
    return 1;
  }
  // stop_discovery();
  ChiakiConnectVideoProfile profile = {};
	chiaki_connect_video_profile_preset(&profile,
		context.config.resolution, context.config.fps);
  // profile.bitrate = 15000;
	// Build chiaki ps4 stream session
	ChiakiAudioSink audio_sink;
	chiaki_opus_decoder_init(&context.stream.opus_decoder, &context.log);
	ChiakiConnectInfo chiaki_connect_info = {};

	chiaki_connect_info.host = host->hostname;
	chiaki_connect_info.video_profile = profile;
	chiaki_connect_info.video_profile_auto_downgrade = true;

	chiaki_connect_info.ps5 = chiaki_target_is_ps5(host->target);

	memcpy(chiaki_connect_info.regist_key, host->registered_state->rp_regist_key, sizeof(chiaki_connect_info.regist_key));
	memcpy(chiaki_connect_info.morning, host->registered_state->rp_key, sizeof(chiaki_connect_info.morning));

	ChiakiErrorCode err = chiaki_session_init(&context.stream.session, &chiaki_connect_info, &context.log);
	if(err != CHIAKI_ERR_SUCCESS) {
		LOGE("Error during stream setup: %s", chiaki_error_string(err));
    return 1;
  }
  init_controller_map(&(context.stream.vcmi), context.config.controller_map_id);
	context.stream.session_init = true;
	// audio setting_cb and frame_cb
	chiaki_opus_decoder_set_cb(&context.stream.opus_decoder, vita_audio_init, vita_audio_cb, NULL);
	chiaki_opus_decoder_get_sink(&context.stream.opus_decoder, &audio_sink);
	chiaki_session_set_audio_sink(&context.stream.session, &audio_sink);
  chiaki_session_set_video_sample_cb(&context.stream.session, video_cb, NULL);
	chiaki_session_set_event_cb(&context.stream.session, event_cb, NULL);

	// init controller states
	chiaki_controller_state_set_idle(&context.stream.controller_state);

  err = vita_h264_setup(profile.width, profile.height);
  if (err != 0) {
		LOGE("Error during video start: %d", err);
    return 1;
  }
  vita_h264_start();

	err = chiaki_session_start(&context.stream.session);
  if(err != CHIAKI_ERR_SUCCESS) {
		LOGE("Error during stream start: %s", chiaki_error_string(err));
    return 1;
  }

	err = chiaki_thread_create(&context.stream.input_thread, input_thread_func, &context.stream);
	if(err != CHIAKI_ERR_SUCCESS)
	{
		LOGE("Failed to create input thread");
	}
  return 0;
}

/// Check if two MAC addresses match
bool mac_addrs_match(MacAddr* a, MacAddr* b) {
  for (int j = 0; j < 6; j++) {
    if ((*a)[j] != (*b)[j]) {
      return false;
    }
  }
  return true;
}

/// Save a new manual host into the context, given existing registered host and new remote ip ("hostname")
void save_manual_host(VitaChiakiHost* rhost, char* new_hostname) {
  // TODO Check if the host already exists, and if not, locate a free spot for it

  // copy mac address
  uint8_t host_mac[6];
  memcpy(&host_mac, &(rhost->server_mac), 6);

  for (int i = 0; i < context.config.num_manual_hosts; i++) {
    VitaChiakiHost* h = context.config.manual_hosts[i];
    CHIAKI_LOGW(&(context.log), "CHECKING MANUAL HOST %d (%s)", i, h->hostname);
      if (strcmp(h->hostname, new_hostname)) {
      CHIAKI_LOGW(&(context.log), "  HOSTNAMES MATCH");
      }
    if (mac_addrs_match(&(h->server_mac), &host_mac)) {
      CHIAKI_LOGW(&(context.log), "  MACS MATCH");
      if (strcmp(h->hostname, new_hostname)) {
        // this manual host already exists (same mac addr and hostname)
        CHIAKI_LOGW(&(context.log), "Duplicate manual host. Not saving.");
        return;
      }
    }
  }

  VitaChiakiHost* newhost = (VitaChiakiHost*)malloc(sizeof(VitaChiakiHost));
  newhost->registered_state = NULL;
  newhost->hostname = strdup(new_hostname);
  memcpy(&(newhost->server_mac), &host_mac, 6);
  newhost->type |= MANUALLY_ADDED;
  // newhost->type |= REGISTERED; // ??
  newhost->target = rhost->target;

  CHIAKI_LOGI(&(context.log), "--");
  CHIAKI_LOGI(&(context.log), "Adding manual host:");

  if(newhost->hostname)
    CHIAKI_LOGI(&(context.log), "Host Name (address):               %s", newhost->hostname);
  if(host_mac)
    CHIAKI_LOGI(&(context.log), "Host MAC:                          %X%X%X%X%X%X\n", host_mac[0], host_mac[1], host_mac[2], host_mac[3], host_mac[4], host_mac[5]);
  CHIAKI_LOGI(&(context.log),   "Is PS5:                            %s", chiaki_target_is_ps5(newhost->target) ? "true" : "false");

  CHIAKI_LOGI(&(context.log), "--");

  if (context.config.num_manual_hosts >= MAX_NUM_HOSTS) { // TODO change to manual max
    CHIAKI_LOGE(&(context.log), "Max manual hosts reached; could not save.");
    return;
  }

  // Save to manual hosts
  context.config.manual_hosts[context.config.num_manual_hosts++] = newhost;

  // Save config
  config_serialize(&context.config);

  // TODO update hosts in context
}
