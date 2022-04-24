#include <psp2/audioout.h>

#include "audio.h"
#include "context.h"
#define FRAME_SIZE 240

int port = -1;
int decode_offset = 0;
int audioRate = 0;

void vita_audio_init(unsigned int channels, unsigned int rate, void *user) {
    if (port == -1) {
        LOGD("vita audio init with %d channels at %dHz", channels, rate);
        audioRate = rate;
        port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, 480, rate, channels == 2 ? SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO : SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO);
	    sceAudioOutSetVolume(port, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, (int[]){SCE_AUDIO_VOLUME_0DB,SCE_AUDIO_VOLUME_0DB});
    }
}

void vita_audio_cb(int16_t *buf, size_t samples_count) {
    for(int x = 0; x < samples_count * 2; x++) {
		// boost audio volume
		int sample = buf[x] * 1.80;
		// Hard clipping (audio compression)
		// truncate value that overflow/underflow int16
		if(sample > INT16_MAX) {
			buf[x] = INT16_MAX;
			LOGD("Audio Hard clipping INT16_MAX < %d", sample);
		}
		else if(sample < INT16_MIN) {
			buf[x] = INT16_MIN;
			LOGD("Audio Hard clipping INT16_MIN > %d", sample);
		}
		else
			buf[x] = (int16_t)sample;
	}

    // if (samples_count > 0) {
        LOGD("samples %d", samples_count);
        // if (samples_count != FRAME_SIZE)
        // return;
        // decode_offset += samples_count;

        // if (decode_offset == audioRate) {
        //     decode_offset = 0;
            sceAudioOutOutput(port, NULL);
            sceAudioOutOutput(port, buf);
        // }
    // }

}