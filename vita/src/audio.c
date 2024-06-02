#include <string.h> // memcpy
#include <stdlib.h>
#include <psp2/audioout.h>


#include "audio.h"
#include "context.h"

#define DEFAULT_INIT_SAMPLE_COUNT 960
#define READ_BUFFERS 4 // number of read buffers in the buffer

// Notes:
// 1. The Vita requires a sample count divisible by 64.
//    In my (ywnico) testing on a PS5, the frame size is 480 (480/64 = 7.5).
//    So we need to create an intermediate buffer of length frame_size * mult,
//    where mult is 2, 4, etc. as needed to make the length divisible by 64.
// 2. Our intermediate buffer is longer than the buffer we send to the audio
//    device. We call the latter the `read_buffer` (sorry for my poor naming
//    skills). So the intermediate buffer (`buffer`) consists of a total of
//    `READ_BUFFERS` read buffers, and each of these consists of
//    mult * frame_size samples in order to be divisible by 64.
//    As we receive data from Chiaki (`buf_in`), we add it onto `buffer`. As
//    each read_buffer segment gets filled out, we send it to the audio device.
// 3. Adjusting the size and number of read buffers affects the sound. If the
//    read buffer is too big, it will be output too slowly and will gradually
//    fall behind the input data (hence the need for a queue clearing maneuver).

int port = -1;
int rate = -1;
int channels = -1;

// on first call to vita_audio_cb, get the actual frame size, re-init audio
// port, and set up intermediate buffer
bool did_secondary_init = false;
size_t frame_size; // # of samples in frame

int16_t* buffer;
size_t buffer_frames; // # of frames in buffer
size_t buffer_samples; // # of samples in buffer
size_t read_buffer_frames; // # of frames in read_buffer (buffer_frames / READ_BUFFERS)
size_t read_buffer_samples; // # of samples in read buffer (buffer_samples / READ_BUFFERS)
size_t sample_bytes; // channels * sizeof(int16_t)
size_t sample_steps; // (=channels) steps to use for array arithmetic
size_t buffer_bytes; // size of buffer in bytes = buffer_samples * sample_bytes

size_t write_frame_offset; // offset in buffer (counted in frames, not bytes or samples)
size_t read_buffer_offset; // offset in buffer (counted in read buffers)
int write_read_framediff;


void vita_audio_init(unsigned int channels_, unsigned int rate_, void *user) {
    if (port == -1) {

        // set globals
        rate = rate_;
        channels = channels_;
        sample_steps = channels;
        sample_bytes = sample_steps * sizeof(int16_t);

        LOGD("VITA AUDIO :: init with %d channels at %dHz", channels, rate);
        port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, DEFAULT_INIT_SAMPLE_COUNT, rate, channels == 2 ? SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO : SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO);
	    if (port < 0) {
            LOGD("VITA AUDIO :: STARTUP ERROR 0x%x", port);
            return;
        }
        sceAudioOutSetVolume(port, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, (int[]){SCE_AUDIO_VOLUME_0DB,SCE_AUDIO_VOLUME_0DB});
    }
}

// determine required buffer size and allocate the required memory
// TODO figure out where/when to free this buffer....
void init_buffer() {
    // set global buffer_frames
    int two_pow = frame_size & ((~frame_size)+1); // highest power of 2 dividing frame_size
    if (two_pow >= 64) {
        // frame_size already divisible by 64
        read_buffer_frames = 1;
    } else {
        read_buffer_frames = 64 / two_pow;
    }

    // set globals
    buffer_frames = read_buffer_frames * READ_BUFFERS;
    buffer_samples = frame_size * buffer_frames;
    buffer_bytes = buffer_samples * sample_bytes;

    read_buffer_samples = frame_size * read_buffer_frames;

    // initialize buffer
    buffer = (int16_t*)malloc(buffer_bytes);
    write_frame_offset = 0;
    read_buffer_offset = 0;
    write_read_framediff = 0;

    LOGD("VITA AUDIO :: buffer init: buffer_frames %d, buffer_samples %d, buffer_bytes %d, frame_size %d, sample_bytes %d", buffer_frames, buffer_samples, buffer_bytes, frame_size, sample_bytes);
}

void vita_audio_cb(int16_t *buf_in, size_t samples_count, void *user) {
    if (!did_secondary_init) {
        frame_size = samples_count;

        init_buffer();

        sceAudioOutSetConfig(port, read_buffer_samples, rate, channels == 2 ? SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO : SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO);

        did_secondary_init = true;
        LOGD("VITA AUDIO :: secondary init complete");
    }

    if (samples_count == frame_size) {

        // write to buffer
        memcpy(buffer + write_frame_offset*frame_size*sample_steps, buf_in, frame_size * sample_bytes);
        write_frame_offset++;
        write_read_framediff++;

        // if we've filled up the buffer, reset write offset
        if (write_frame_offset == buffer_frames) {
            write_frame_offset = 0;
        }

        if (write_read_framediff >= read_buffer_frames) {

            bool should_output = false;
            if (write_read_framediff >= read_buffer_frames*(READ_BUFFERS-1)) {
                // if the incoming buffers (write) have gotten too far ahead of the
                // audio (read), just skip ahead. This should be analogous to the
                // queue clearing in the nintendo switch code.
                // TODO check this math
                LOGD("VITA AUDIO :: audio catchup: [before] write_read_framediff %d, write_frame_offset %d, read_buffer_offset %d", write_read_framediff, write_frame_offset, read_buffer_offset);
                read_buffer_offset = ((write_frame_offset + 1)/read_buffer_frames - 1 + READ_BUFFERS) % READ_BUFFERS;
                write_read_framediff = read_buffer_frames + write_frame_offset % read_buffer_frames;
                LOGD("VITA AUDIO :: audio catchup: [after]  write_read_framediff %d, write_frame_offset %d, read_buffer_offset %d", write_read_framediff, write_frame_offset, read_buffer_offset);
                should_output = true;
            } else {
                // otherwise, tell the device to output only if its already clear
                // TODO do we really need to wait for remaining_samples = 0 to send audio?
                int remaining_samples = sceAudioOutGetRestSample(port);
                if (remaining_samples == 0) should_output = true;
            }

            if (should_output) {
                sceAudioOutOutput(port, buffer + read_buffer_offset*read_buffer_samples*sample_steps);
                read_buffer_offset = (read_buffer_offset + 1) % READ_BUFFERS;
                write_read_framediff -= read_buffer_frames;
                //LOGD("VITA AUDIO :: Vita audio output write_read_framediff: %d", write_read_framediff);
            }
        }

    } else {
        LOGD("VITA AUDIO :: Expected %d (frame_size) samples but received %d.", frame_size, samples_count);
    }

}
