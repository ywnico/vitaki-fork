#include "context.h"
#include "config.h"
#include "host.h"
#include "discovery.h"
#include "audio.h"
#include "video.h"
#include "string.h"
#include "controller.h"
#include <stdio.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <chiaki/base64.h>
#include <chiaki/session.h>

void host_init(VitaChiakiHost* host) {

  // TODO set up alternate controller maps
  memset(vitaki_ctrl_in_out_map, 0, VITAKI_CTRL_IN_COUNT);
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_L1]                  = VITAKI_CTRL_OUT_L1;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_R1]                  = VITAKI_CTRL_OUT_R1;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_L3]                  = VITAKI_CTRL_OUT_L3;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_R3]                  = VITAKI_CTRL_OUT_R3;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_SELECT_START]        = VITAKI_CTRL_OUT_PS;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_LEFT_SQUARE]         = VITAKI_CTRL_OUT_L3;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_RIGHT_CIRCLE]        = VITAKI_CTRL_OUT_R3;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_REARTOUCH_UL]        = VITAKI_CTRL_OUT_NONE;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_REARTOUCH_UR]        = VITAKI_CTRL_OUT_NONE;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_REARTOUCH_LL]        = VITAKI_CTRL_OUT_NONE;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_REARTOUCH_LR]        = VITAKI_CTRL_OUT_NONE;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_REARTOUCH_LEFT]      = VITAKI_CTRL_OUT_NONE;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_REARTOUCH_RIGHT]     = VITAKI_CTRL_OUT_NONE;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_REARTOUCH_ANY]       = VITAKI_CTRL_OUT_NONE;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_REARTOUCH_LEFT_L1]   = VITAKI_CTRL_OUT_L2;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_REARTOUCH_RIGHT_R1]  = VITAKI_CTRL_OUT_R2;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_FRONTTOUCH_UL]       = VITAKI_CTRL_OUT_NONE;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_FRONTTOUCH_UR]       = VITAKI_CTRL_OUT_NONE;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_FRONTTOUCH_LL]       = VITAKI_CTRL_OUT_NONE;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_FRONTTOUCH_LR]       = VITAKI_CTRL_OUT_NONE;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_FRONTTOUCH_LEFT]     = VITAKI_CTRL_OUT_NONE;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_FRONTTOUCH_RIGHT]    = VITAKI_CTRL_OUT_NONE;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_FRONTTOUCH_ANY]      = VITAKI_CTRL_OUT_TOUCHPAD;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_FRONTTOUCH_LEFT_L1]  = VITAKI_CTRL_OUT_NONE;
  vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_FRONTTOUCH_RIGHT_R1] = VITAKI_CTRL_OUT_NONE;
  
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
  if (ctrl_out == VITAKI_CTRL_OUT_NONE) {
    // do nothing
  } else if (ctrl_out == VITAKI_CTRL_OUT_L2) {
    stream->controller_state.l2_state = 0xff;
  } else if (ctrl_out == VITAKI_CTRL_OUT_R2) {
    stream->controller_state.r2_state = 0xff;
  } else {
    // in this case ctrl_out is a controller button mask
    stream->controller_state.buttons |= ctrl_out;
  }
}

static void *input_thread_func(void* user) {
  sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
  sceCtrlSetSamplingModeExt(SCE_CTRL_MODE_ANALOG_WIDE);
  SceCtrlData ctrl;
	VitaChiakiStream *stream = user;
  int ms_per_loop = 5;

  // Touchscreen setup
	sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
	sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);
	sceTouchEnableTouchForce(SCE_TOUCH_PORT_FRONT);
	sceTouchEnableTouchForce(SCE_TOUCH_PORT_BACK);
	SceTouchData touch[SCE_TOUCH_PORT_MAX_NUM];
  int TOUCH_MAX_WIDTH = 1919;
  int TOUCH_MAX_HEIGHT = 1087;
  // note: rear touch may be active in Y only from 108 to 889? (see Vita3K code)

  while (true) {

    memset(vitaki_ctrl_in_state, 0, VITAKI_CTRL_IN_COUNT);

    // TODO enable using triggers as L2, R2
    // TODO enable home button, with long hold sent back to Vita?



    if (stream->is_streaming) {
      // get button state
      sceCtrlPeekBufferPositiveExt2(0, &ctrl, 1);

      // get touchscreen state
      for(int port = 0; port < SCE_TOUCH_PORT_MAX_NUM; port++) {
        sceTouchPeek(port, &touch[port], 1);
      }

      for (int touch_i = 0; touch_i < touch[SCE_TOUCH_PORT_BACK].reportNum; touch_i++) {
        int x = touch[SCE_TOUCH_PORT_BACK].report[touch_i].x;
        int y = touch[SCE_TOUCH_PORT_BACK].report[touch_i].y;
        vitaki_ctrl_in_state[VITAKI_CTRL_IN_REARTOUCH_ANY] = true;

        if (x > TOUCH_MAX_WIDTH/2) {
          vitaki_ctrl_in_state[VITAKI_CTRL_IN_REARTOUCH_RIGHT] = true;
          if (y > TOUCH_MAX_HEIGHT/2) {
            vitaki_ctrl_in_state[VITAKI_CTRL_IN_REARTOUCH_UR] = true;
          } else if (y < TOUCH_MAX_HEIGHT/2) {
            vitaki_ctrl_in_state[VITAKI_CTRL_IN_REARTOUCH_LR] = true;
          }
        } else if (x < TOUCH_MAX_WIDTH/2) {
          vitaki_ctrl_in_state[VITAKI_CTRL_IN_REARTOUCH_LEFT] = true;
          if (y > TOUCH_MAX_HEIGHT/2) {
            vitaki_ctrl_in_state[VITAKI_CTRL_IN_REARTOUCH_UL] = true;
          } else if (y < TOUCH_MAX_HEIGHT/2) {
            vitaki_ctrl_in_state[VITAKI_CTRL_IN_REARTOUCH_LL] = true;
          }
        }
      }

      for (int touch_i = 0; touch_i < touch[SCE_TOUCH_PORT_FRONT].reportNum; touch_i++) {
        int x = touch[SCE_TOUCH_PORT_FRONT].report[touch_i].x;
        int y = touch[SCE_TOUCH_PORT_FRONT].report[touch_i].y;
        vitaki_ctrl_in_state[VITAKI_CTRL_IN_FRONTTOUCH_ANY] = true;

        if (x > TOUCH_MAX_WIDTH/2) {
          vitaki_ctrl_in_state[VITAKI_CTRL_IN_FRONTTOUCH_RIGHT] = true;
          if (y > TOUCH_MAX_HEIGHT/2) {
            vitaki_ctrl_in_state[VITAKI_CTRL_IN_FRONTTOUCH_UR] = true;
          } else if (y < TOUCH_MAX_HEIGHT/2) {
            vitaki_ctrl_in_state[VITAKI_CTRL_IN_FRONTTOUCH_LR] = true;
          }
        } else if (x < TOUCH_MAX_WIDTH/2) {
          vitaki_ctrl_in_state[VITAKI_CTRL_IN_FRONTTOUCH_LEFT] = true;
          if (y > TOUCH_MAX_HEIGHT/2) {
            vitaki_ctrl_in_state[VITAKI_CTRL_IN_FRONTTOUCH_UL] = true;
          } else if (y < TOUCH_MAX_HEIGHT/2) {
            vitaki_ctrl_in_state[VITAKI_CTRL_IN_FRONTTOUCH_LL] = true;
          }
        }
      }


      // 0-255 conversion
      stream->controller_state.left_x = (ctrl.lx - 128) * 2 * 0x7F/*.FF*/;
      stream->controller_state.left_y = (ctrl.ly - 128) * 2 * 0x7F/*.FF*/;
      stream->controller_state.right_x = (ctrl.rx - 128) * 2 * 0x7F/*.FF*/;
      stream->controller_state.right_y = (ctrl.ry - 128) * 2 * 0x7F/*.FF*/;

      stream->controller_state.buttons = 0x00;
      stream->controller_state.l2_state = 0x00;
      stream->controller_state.r2_state = 0x00;

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
      if (ctrl.buttons & SCE_CTRL_L3) vitaki_ctrl_in_state[VITAKI_CTRL_IN_L3] = true;
      if (ctrl.buttons & SCE_CTRL_R3) vitaki_ctrl_in_state[VITAKI_CTRL_IN_R3] = true;
      if (ctrl.buttons & SCE_CTRL_L1) vitaki_ctrl_in_state[VITAKI_CTRL_IN_L1] = true;
      if (ctrl.buttons & SCE_CTRL_R1) vitaki_ctrl_in_state[VITAKI_CTRL_IN_R1] = true;

      // Select + Start
      if (vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_SELECT_START]) {
        if ((ctrl.buttons & SCE_CTRL_SELECT) && (ctrl.buttons & SCE_CTRL_START)) {
          stream->controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_SHARE;
          stream->controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_OPTIONS;
          vitaki_ctrl_in_state[VITAKI_CTRL_IN_SELECT_START] = true;
        }
      }

      // Rear touch screen left + L1 = L2
      if (vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_REARTOUCH_LEFT_L1]) {
        if (vitaki_ctrl_in_state[VITAKI_CTRL_IN_REARTOUCH_LEFT] && (ctrl.buttons & SCE_CTRL_L1)) {
          stream->controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_L1;
          vitaki_ctrl_in_state[VITAKI_CTRL_IN_REARTOUCH_LEFT_L1] = true;
        }
      }
      // Rear touch screen right + R1 = R2
      if (vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_REARTOUCH_RIGHT_R1]) {
        if (vitaki_ctrl_in_state[VITAKI_CTRL_IN_REARTOUCH_RIGHT] && (ctrl.buttons & SCE_CTRL_R1)) {
          stream->controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_R1;
          vitaki_ctrl_in_state[VITAKI_CTRL_IN_REARTOUCH_RIGHT_R1] = true;
        }
      }

      // Dpad-left + Square = left analog push
      if (vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_LEFT_SQUARE]) {
        if ((ctrl.buttons & SCE_CTRL_LEFT) && (ctrl.buttons & SCE_CTRL_SQUARE)) {
          stream->controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT;
          stream->controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_BOX;
          vitaki_ctrl_in_state[VITAKI_CTRL_IN_LEFT_SQUARE] = true;
        }
      }
      // Dpad-right + Circle = right analog push
      if (vitaki_ctrl_in_out_map[VITAKI_CTRL_IN_RIGHT_CIRCLE]) {
        if ((ctrl.buttons & SCE_CTRL_RIGHT) && (ctrl.buttons & SCE_CTRL_CIRCLE)) {
          stream->controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT;
          stream->controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_MOON;
          vitaki_ctrl_in_state[VITAKI_CTRL_IN_RIGHT_CIRCLE] = true;
        }
      }

      // Set controls according to vitaki_ctrl_in_out_map
      for (int vitaki_ctrl_in_j = 0; vitaki_ctrl_in_j < VITAKI_CTRL_IN_COUNT; vitaki_ctrl_in_j++) {
        if (vitaki_ctrl_in_state[vitaki_ctrl_in_j]) {
          set_ctrl_out(stream, vitaki_ctrl_in_out_map[vitaki_ctrl_in_j]);
        }
      }

      chiaki_session_set_controller_state(&stream->session, &stream->controller_state);
      // LOGD("ly 0x%x %d", ctrl.ly, ctrl.ly);
      usleep(ms_per_loop*1000);
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
