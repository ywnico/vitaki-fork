#include "context.h"
#include "config.h"
#include "host.h"
#include "discovery.h"
#include "audio.h"
#include "video.h"
#include "string.h"
#include <stdio.h>
#include <psp2/ctrl.h>
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

static void *input_thread_func(void* user) {
  sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
  sceCtrlSetSamplingModeExt(SCE_CTRL_MODE_ANALOG_WIDE);
  SceCtrlData ctrl;
	VitaChiakiStream *stream = user;
  while (true) {
    if (stream->is_streaming) {
      sceCtrlPeekBufferPositiveExt2(0, &ctrl, 1);
      // 0-255 conversion
      stream->controller_state.left_x = (ctrl.lx - 128) * 2 * 0x7F/*.FF*/;
      stream->controller_state.left_y = (ctrl.ly - 128) * 2 * 0x7F/*.FF*/;
      stream->controller_state.right_x = (ctrl.rx - 128) * 2 * 0x7F/*.FF*/;
      stream->controller_state.right_y = (ctrl.ry - 128) * 2 * 0x7F/*.FF*/;
      stream->controller_state.buttons = 0x00;
      // cursed conversion
      if (ctrl.buttons & SCE_CTRL_SELECT) stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_SHARE;
      if (ctrl.buttons & SCE_CTRL_L3) stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_L3;
      if (ctrl.buttons & SCE_CTRL_R3) stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_R3;
      if (ctrl.buttons & SCE_CTRL_START) stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_OPTIONS;
      if (ctrl.buttons & SCE_CTRL_UP) stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_UP;
      if (ctrl.buttons & SCE_CTRL_RIGHT) stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT;
      if (ctrl.buttons & SCE_CTRL_DOWN) stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN;
      if (ctrl.buttons & SCE_CTRL_LEFT) stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT;
      if (ctrl.buttons & SCE_CTRL_L1) stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_L1;
      if (ctrl.buttons & SCE_CTRL_R1) stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_R1;
      if (ctrl.buttons & SCE_CTRL_TRIANGLE) stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_PYRAMID;
      if (ctrl.buttons & SCE_CTRL_CIRCLE) stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_MOON;
      if (ctrl.buttons & SCE_CTRL_CROSS) stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_CROSS;
      if (ctrl.buttons & SCE_CTRL_SQUARE) stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_BOX;
      if ((ctrl.buttons & SCE_CTRL_SELECT) && (ctrl.buttons & SCE_CTRL_START)) {
        stream->controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_SHARE;
        stream->controller_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_OPTIONS;
        stream->controller_state.buttons |= CHIAKI_CONTROLLER_BUTTON_PS;
      }

      chiaki_session_set_controller_state(&stream->session, &stream->controller_state);
      // LOGD("ly 0x%x %d", ctrl.ly, ctrl.ly);
      usleep(5000); // 5 ms
    }
  }

  return 0;
}

int host_stream(VitaChiakiHost* host) {
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