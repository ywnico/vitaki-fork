#include "context.h"
#include "host.h"
#include "video.h"
#include "string.h"

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
    context.active_host->registered_state = event->registered_host;
    context.active_host->type |= REGISTERED;
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
  // TODO: Wake up host
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
      vita_h264_cleanup();
      context.stream.is_streaming = false;
			break;
	}
}

static bool video_cb(uint8_t *buf, size_t buf_size, void *user) {
  context.stream.is_streaming = true;
  int err = vita_h264_decode_frame(buf, buf_size);
  if (err != 0) {
		LOGE("Error during video decode: %d", err);
    // return 1;
  }
  return true;
}

int host_stream(VitaChiakiHost* host) {
  if (!host->hostname || !host->registered_state) {
    return 1;
  }
  ChiakiConnectVideoProfile profile = {};
	chiaki_connect_video_profile_preset(&profile,
		CHIAKI_VIDEO_RESOLUTION_PRESET_540p, CHIAKI_VIDEO_FPS_PRESET_30);
	// Build chiaki ps4 stream session
	// chiaki_opus_decoder_init(&(this->opus_decoder), this->log);
	// ChiakiAudioSink audio_sink;
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
	// chiaki_opus_decoder_set_cb(&this->opus_decoder, InitAudioCB, AudioCB, user);
	// chiaki_opus_decoder_get_sink(&this->opus_decoder, &audio_sink);
	// chiaki_session_set_audio_sink(&this->session, &audio_sink);
  chiaki_session_set_video_sample_cb(&context.stream.session, video_cb, NULL);
	chiaki_session_set_event_cb(&context.stream.session, event_cb, NULL);

	// init controller states
	chiaki_controller_state_set_idle(&context.stream.controller_state);


  err = vita_h264_setup(960, 540);
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