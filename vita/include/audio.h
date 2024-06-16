#pragma once

void vita_audio_init(unsigned int channels, unsigned int rate, void *user);

void vita_audio_cb(int16_t *buf, size_t samples_count, void *user);

void vita_audio_cleanup();
