/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2016 Ilya Zhuravlev
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#include "video.h"
#include "context.h"

#include <chiaki/thread.h>

#include <stdbool.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/display.h>
#include <psp2/videodec.h>
#include <vita2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdarg.h>

void draw_streaming(vita2d_texture *frame_texture);
void draw_fps();
void draw_indicators();

enum {
  VITA_VIDEO_INIT_OK                    = 0,
  VITA_VIDEO_ERROR_NO_MEM               = 0x80010001,
  VITA_VIDEO_ERROR_INIT_LIB             = 0x80010002,
  VITA_VIDEO_ERROR_QUERY_DEC_MEMSIZE    = 0x80010003,
  VITA_VIDEO_ERROR_ALLOC_MEM            = 0x80010004,
  VITA_VIDEO_ERROR_GET_MEMBASE          = 0x80010005,
  VITA_VIDEO_ERROR_CREATE_DEC           = 0x80010006,
  VITA_VIDEO_ERROR_CREATE_PACER_THREAD  = 0x80010007,
};

// #define DECODER_BUFFER_SIZE (92 * 1024)
// #define DECODER_BUFFER_SIZE 0x800000

// static char* decoder_buffer = NULL;

enum {
  SCREEN_WIDTH = 960,
  SCREEN_HEIGHT = 544,
  LINE_SIZE = 960,
  FRAMEBUFFER_SIZE = 2 * 1024 * 1024,
  FRAMEBUFFER_ALIGNMENT = 256 * 1024
};

enum VideoStatus {
  NOT_INIT,
  INIT_GS,
  INIT_FRAMEBUFFER,
  INIT_AVC_LIB,
  INIT_DECODER_MEMBLOCK,
  INIT_AVC_DEC,
  INIT_FRAME_PACER_THREAD,
};

vita2d_texture *frame_texture = NULL;
enum VideoStatus video_status = NOT_INIT;

SceAvcdecCtrl *decoder = NULL;
SceUID displayblock = -1;
SceUID decoderblock = -1;
SceUID pacer_thread = -1;
SceVideodecQueryInitInfoHwAvcdec *init = NULL;
SceAvcdecQueryDecoderInfo *decoder_info = NULL;

typedef struct {
  bool activated;
  uint8_t alpha;
  bool plus;
} indicator_status;

static unsigned numframes;
static bool active_video_thread = true;
static bool active_pacer_thread = false;
static indicator_status poor_net_indicator = {0};

uint32_t frame_count = 0;
uint32_t need_drop = 0;
uint32_t curr_fps[2] = {0, 0};
float carry = 0;

typedef struct {
  unsigned int texture_width;
  unsigned int texture_height;
  float origin_x;
  float origin_y;
  float region_x1;
  float region_y1;
  float region_x2;
  float region_y2;
} image_scaling_settings;

static image_scaling_settings image_scaling = {0};

void update_scaling_settings(int width, int height) {
  image_scaling.texture_width = SCREEN_WIDTH;
  image_scaling.texture_height = SCREEN_HEIGHT;
  image_scaling.origin_x = 0;
  image_scaling.origin_y = 0;
  image_scaling.region_x1 = 0;
  image_scaling.region_y1 = 0;
  image_scaling.region_x2 = image_scaling.texture_width;
  image_scaling.region_y2 = image_scaling.texture_height;

  double scaled_width = (double) SCREEN_HEIGHT * width / height;
  double scaled_height = (double) SCREEN_WIDTH * height / width;

  if (SCREEN_WIDTH * height == SCREEN_HEIGHT * width) {
    // streaming resolution ratio matches Vita's screen ratio
    // use default setting
  } else if (SCREEN_WIDTH * height > SCREEN_HEIGHT * width) {
    // host ratio example: 4:3, 16:10
    // Vita ratio range: 2:16 (64 x 544) - native (960 x 544)
    if (false/*config.center_region_only*/) {
      image_scaling.texture_height = VITA_DECODER_RESOLUTION(scaled_height);
      image_scaling.region_y1 = VITA_DECODER_RESOLUTION((scaled_height - SCREEN_HEIGHT) / 2);
      image_scaling.region_y2 = VITA_DECODER_RESOLUTION((scaled_height + SCREEN_HEIGHT) / 2);
    } else {
      image_scaling.texture_width = VITA_DECODER_RESOLUTION(scaled_width);
      image_scaling.region_x2 = VITA_DECODER_RESOLUTION(scaled_width);
      image_scaling.origin_x = round((double) (SCREEN_WIDTH - image_scaling.texture_width) / 2);
    }
  } else {
    // host ratio example: 16:9, 21:9, 32:9
    // Vita ratio range: native (960 x 544) - 15:1 (960 x 64)
    if (false/*config.center_region_only*/) {
      image_scaling.texture_width = VITA_DECODER_RESOLUTION(scaled_width);
      image_scaling.region_x1 = VITA_DECODER_RESOLUTION((scaled_width - SCREEN_WIDTH) / 2);
      image_scaling.region_x2 = VITA_DECODER_RESOLUTION((scaled_width + SCREEN_WIDTH) / 2);
    } else {
      image_scaling.texture_height = VITA_DECODER_RESOLUTION(scaled_height);
      image_scaling.region_y2 = VITA_DECODER_RESOLUTION(scaled_height);
      image_scaling.origin_y = round((double) (SCREEN_HEIGHT - image_scaling.texture_height) / 2);
    }
  }

  LOGD("update_scaling_settings: width = %u\n", width);
  LOGD("update_scaling_settings: height = %u\n", height);
  LOGD("update_scaling_settings: scaled_width = %f\n", scaled_width);
  LOGD("update_scaling_settings: scaled_height = %f\n", scaled_height);
  LOGD("update_scaling_settings: image_scaling.texture_width = %u\n", image_scaling.texture_width);
  LOGD("update_scaling_settings: image_scaling.texture_height = %u\n", image_scaling.texture_height);
  LOGD("update_scaling_settings: image_scaling.origin_x = %f\n", image_scaling.origin_x);
  LOGD("update_scaling_settings: image_scaling.origin_y = %f\n", image_scaling.origin_y);
  LOGD("update_scaling_settings: image_scaling.region_x1 = %f\n", image_scaling.region_x1);
  LOGD("update_scaling_settings: image_scaling.region_y1 = %f\n", image_scaling.region_y1);
  LOGD("update_scaling_settings: image_scaling.region_x2 = %f\n", image_scaling.region_x2);
  LOGD("update_scaling_settings: image_scaling.region_y2 = %f\n", image_scaling.region_y2);
}

static int vita_pacer_thread_main(SceSize args, void *argp) {
  // 1s
  int wait = 1000000;
  //float max_fps = 0;
  //sceDisplayGetRefreshRate(&max_fps);
  //if (config.stream.fps == 30) {
  //  max_fps /= 2;
  //}
  int max_fps = context.stream.fps;
  uint64_t last_vblank_count = sceDisplayGetVcount();
  uint64_t last_check_time = sceKernelGetSystemTimeWide();
  //float carry = 0;
  need_drop = 0;
  frame_count = 0;
  while (active_pacer_thread) {
    uint64_t curr_vblank_count = sceDisplayGetVcount();
    uint32_t vblank_fps = curr_vblank_count - last_vblank_count;
    uint32_t curr_frame_count = frame_count;
    frame_count = 0;

    if (!active_video_thread) {
    //  carry = 0;
    LOGD("thread inactive");
    } else {
      if (/*config.enable_frame_pacer*/false && curr_frame_count > max_fps) {
        //carry += curr_frame_count - max_fps;
        //if (carry > 1) {
        //  need_drop += (int)carry;
        //  carry -= (int)carry;
        //}
        need_drop += curr_frame_count - max_fps;
      }
      LOGD("fps0/fps1/carry/need_drop: %u/%u/%f/%u\n",
                    curr_frame_count, vblank_fps, carry, need_drop);
    }

    curr_fps[0] = curr_frame_count;
    curr_fps[1] = vblank_fps;

    last_vblank_count = curr_vblank_count;
    uint64_t curr_check_time = sceKernelGetSystemTimeWide();
    uint32_t lapse = curr_check_time - last_check_time;
    last_check_time = curr_check_time;
    if (lapse > wait && (lapse - wait) < wait) {
      LOGD("sleep: %d wait: %d lapse: %d", wait * 2 - lapse, wait, lapse);
      sceKernelDelayThread(wait * 2 - lapse);
    } else {
      sceKernelDelayThread(wait);
    }
  }
  return 0;
}

ChiakiMutex mtx;

void vita_h264_cleanup() {
  if (video_status == INIT_FRAME_PACER_THREAD) {
    active_pacer_thread = false;
    // wait 10sec
    SceUInt timeout = 10000000;
    int ret;
    sceKernelWaitThreadEnd(pacer_thread, &ret, &timeout);
    sceKernelDeleteThread(pacer_thread);
    video_status--;
  }

  if (video_status == INIT_AVC_DEC) {
    sceAvcdecDeleteDecoder(decoder);
    video_status--;
  }

  if (video_status == INIT_DECODER_MEMBLOCK) {
    if (decoderblock >= 0) {
      sceKernelFreeMemBlock(decoderblock);
      decoderblock = -1;
    }
    if (decoder != NULL) {
      free(decoder);
      decoder = NULL;
    }
    if (decoder_info != NULL) {
      free(decoder_info);
      decoder_info = NULL;
    }
    video_status--;
  }

  if (video_status == INIT_AVC_LIB) {
    sceVideodecTermLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC);

    if (init != NULL) {
      free(init);
      init = NULL;
    }
    video_status--;
  }

  if (video_status == INIT_FRAMEBUFFER) {
    if (frame_texture != NULL) {
      vita2d_free_texture(frame_texture);
      frame_texture = NULL;
    }

    // if (decoder_buffer != NULL) {
    //   free(decoder_buffer);
    //   decoder_buffer = NULL;
    // }
    video_status--;
  }

  if (video_status == INIT_GS) {
    // gs_sps_stop();
    video_status--;
  }
  chiaki_mutex_fini(&mtx);
}
int vita_h264_setup(int width, int height) {
  int ret;
  LOGD("vita video setup\n");
  chiaki_mutex_init(&mtx, false);

  if (video_status == NOT_INIT) {
    // INIT_GS
    // gs_sps_init(width, height);
    video_status++;
  }

  if (video_status == INIT_GS) {
    // INIT_FRAMEBUFFER
    update_scaling_settings(width, height);

    // decoder_buffer = malloc(DECODER_BUFFER_SIZE);
    // if (decoder_buffer == NULL) {
    //   LOGD("not enough memory\n");
    //   ret = VITA_VIDEO_ERROR_NO_MEM;
    //   goto cleanup;
    // }

    frame_texture = vita2d_create_empty_texture_format(image_scaling.texture_width, image_scaling.texture_height, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR);
    if (frame_texture == NULL) {
      LOGD("not enough memory\n");
      ret = VITA_VIDEO_ERROR_NO_MEM;
      goto cleanup;
    }

    video_status++;
  }

  if (video_status == INIT_FRAMEBUFFER) {
    // INIT_AVC_LIB
    if (init == NULL) {
      init = calloc(1, sizeof(SceVideodecQueryInitInfoHwAvcdec));
      if (init == NULL) {
        LOGD("not enough memory\n");
        ret = VITA_VIDEO_ERROR_NO_MEM;
        goto cleanup;
      }
    }
    init->size = sizeof(SceVideodecQueryInitInfoHwAvcdec);
    init->horizontal = VITA_DECODER_RESOLUTION(width);
    init->vertical = VITA_DECODER_RESOLUTION(height);
    init->numOfRefFrames = 5;
    init->numOfStreams = 1;

    ret = sceVideodecInitLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC, init);
    if (ret < 0) {
      LOGD("sceVideodecInitLibrary 0x%x\n", ret);
      ret = VITA_VIDEO_ERROR_INIT_LIB;
      goto cleanup;
    }
    video_status++;
  }

  if (video_status == INIT_AVC_LIB) {
    // INIT_DECODER_MEMBLOCK
    if (decoder_info == NULL) {
      decoder_info = calloc(1, sizeof(SceAvcdecQueryDecoderInfo));
      if (decoder_info == NULL) {
        LOGD("not enough memory\n");
        ret = VITA_VIDEO_ERROR_NO_MEM;
        goto cleanup;
      }
    }
    decoder_info->horizontal = init->horizontal;
    decoder_info->vertical = init->vertical;
    decoder_info->numOfRefFrames = init->numOfRefFrames;

    SceAvcdecDecoderInfo decoder_info_out = {0};

    ret = sceAvcdecQueryDecoderMemSize(SCE_VIDEODEC_TYPE_HW_AVCDEC, decoder_info, &decoder_info_out);
    if (ret < 0) {
      LOGD("sceAvcdecQueryDecoderMemSize 0x%x size 0x%x\n", ret, decoder_info_out.frameMemSize);
      ret = VITA_VIDEO_ERROR_QUERY_DEC_MEMSIZE;
      goto cleanup;
    }

    decoder = calloc(1, sizeof(SceAvcdecCtrl));
    if (decoder == NULL) {
      LOGD("not enough memory\n");
      ret = VITA_VIDEO_ERROR_ALLOC_MEM;
      goto cleanup;
    }

    size_t sz = (decoder_info_out.frameMemSize + 0xFFFFF) & ~0xFFFFF;
    decoder->frameBuf.size = sz;
    LOGD("allocating size 0x%x\n", sz);
    SceKernelAllocMemBlockOpt *opt;
    opt = calloc(1, sizeof(SceKernelAllocMemBlockOpt));
    opt->size      = sizeof(SceKernelAllocMemBlockOpt);
    opt->attr      = 4;
    opt->alignment = 1024*1024; 
    decoderblock = sceKernelAllocMemBlock("decoder", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, sz, opt);
    // decoderblock = sceKernelAllocMemBlock("decoder", SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW, sz, NULL);
    if (decoderblock < 0) {
      LOGD("decoderblock: 0x%08x\n", decoderblock);
      ret = VITA_VIDEO_ERROR_ALLOC_MEM;
      goto cleanup;
    }

    ret = sceKernelGetMemBlockBase(decoderblock, &decoder->frameBuf.pBuf);
    if (ret < 0) {
      LOGD("sceKernelGetMemBlockBase: 0x%x\n", ret);
      ret = VITA_VIDEO_ERROR_GET_MEMBASE;
      goto cleanup;
    }
    video_status++;
  }

  if (video_status == INIT_DECODER_MEMBLOCK) {
    // INIT_AVC_DEC
    LOGD("base: 0x%08x\n", decoder->frameBuf.pBuf);

    ret = sceAvcdecCreateDecoder(SCE_VIDEODEC_TYPE_HW_AVCDEC, decoder, decoder_info);
    if (ret < 0) {
      LOGD("sceAvcdecCreateDecoder 0x%x\n", ret);
      ret = VITA_VIDEO_ERROR_CREATE_DEC;
      goto cleanup;
    }
    video_status++;
  }

  if (video_status == INIT_AVC_DEC) {
    // INIT_FRAME_PACER_THREAD
    ret = sceKernelCreateThread("frame_pacer", vita_pacer_thread_main, 0, 0x10000, 0, 0, NULL);
    if (ret < 0) {
      LOGD("sceKernelCreateThread 0x%x\n", ret);
      ret = VITA_VIDEO_ERROR_CREATE_PACER_THREAD;
      goto cleanup;
    }
    pacer_thread = ret;
    active_pacer_thread = true;
    sceKernelStartThread(pacer_thread, 0, NULL);
    video_status++;
  }

  return VITA_VIDEO_INIT_OK;

cleanup:
  vita_h264_cleanup();
  return ret;
}
// uint8_t *lbuf;
int vita_h264_decode_frame(uint8_t *buf, size_t buf_size) {
  // free(lbuf);
  // lbuf = buf;
  chiaki_mutex_lock(&mtx);


  SceAvcdecAu au = {0};
  SceAvcdecArrayPicture array_picture = {0};
  struct SceAvcdecPicture picture = {0};
  struct SceAvcdecPicture *pictures = { &picture };
  array_picture.numOfElm = 1;
  array_picture.pPicture = &pictures;

  //frame->time = decodeUnit->receiveTimeMs;

  picture.size = sizeof(picture);
  picture.frame.pixelType = 0;
  picture.frame.framePitch = image_scaling.texture_width;
  picture.frame.frameWidth = image_scaling.texture_width;
  picture.frame.frameHeight = image_scaling.texture_height;
  picture.frame.pPicture[0] = vita2d_texture_get_datap(frame_texture);

  // if (buf_size >= DECODER_BUFFER_SIZE) {
  //   LOGD("Video decode buffer too small\n");
  //   exit(1);
  // }

  // PLENTRY entry = decodeUnit->bufferList;
  // uint32_t length = 0;
  // while (entry != NULL) {
    // if (entry->bufferType == BUFFER_TYPE_SPS) {
      // gs_sps_fix(entry, GS_SPS_BITSTREAM_FIXUP, decoder_buffer, &length);
    // } else {
      // memset(decoder_buffer, 0, DECODER_BUFFER_SIZE);
      // memcpy(decoder_buffer, buf, buf_size);
      // memset(buf, buf_size);
      // free(buf);
      // decoder_buffer = buf;
      // length += entry->length;
    // }
    // entry = entry->next;
  // }

  au.es.pBuf = buf;
  au.es.size = buf_size;
  au.dts.lower = 0xFFFFFFFF;
  au.dts.upper = 0xFFFFFFFF;
  au.pts.lower = 0xFFFFFFFF;
  au.pts.upper = 0xFFFFFFFF;

  SceInt32 avail = sceAvcdecDecodeAvailableSize(decoder);
  LOGD("AvailableSize 0x%x BufSize 0x%x", avail, buf_size);
  int ret = 0;
  ret = sceAvcdecDecode(decoder, &au, &array_picture);
  if (ret < 0) {
    LOGD("sceAvcdecDecode (len=0x%x): 0x%x numOfOutput %d\n", buf_size, ret, array_picture.numOfOutput);
    return 1;
  }

  if (array_picture.numOfOutput != 1) {
    LOGD("numOfOutput %d bufSize 0x%x\n", array_picture.numOfOutput, buf_size);
    return 0;
  }

  if (active_video_thread) {
    if (need_drop > 0) {
      LOGD("remain frameskip: %d\n", need_drop);
      // skip
      need_drop--;
    } else {
      vita2d_start_drawing();

      draw_streaming(frame_texture);
      draw_fps();
      draw_indicators();

      vita2d_end_drawing();

      vita2d_wait_rendering_done();
      vita2d_swap_buffers();

      frame_count++;
      LOGD("frc: %d", frame_count);
    }
  } else {
    LOGD("inactive video thread");
  }

  // if (numframes++ % 6 == 0)
  //   return DR_NEED_IDR;
  chiaki_mutex_unlock(&mtx);
  return 0;
}

void draw_streaming(vita2d_texture *frame_texture) {
  // ui is still rendering in the background, clear the screen first
  vita2d_clear_screen();
  vita2d_draw_texture_part(frame_texture,
                           image_scaling.origin_x,
                           image_scaling.origin_y,
                           image_scaling.region_x1,
                           image_scaling.region_y1,
                           image_scaling.region_x2,
                           image_scaling.region_y2);
}

void draw_fps() {
//   if (config.show_fps) {
//     vita2d_font_draw_textf(font, 40, 20, RGBA8(0xFF, 0xFF, 0xFF, 0xFF), 16, "fps: %u / %u", curr_fps[0], curr_fps[1]);
//   }
}

void draw_indicators() {
//   if (poor_net_indicator.activated) {
//     vita2d_font_draw_text(font, 40, 500, RGBA8(0xFF, 0xFF, 0xFF, poor_net_indicator.alpha), 64, ICON_NETWORK);
//     poor_net_indicator.alpha += (0x4 * (poor_net_indicator.plus ? 1 : -1));
//     if (poor_net_indicator.alpha == 0) {
//       poor_net_indicator.plus = !poor_net_indicator.plus;
//       poor_net_indicator.alpha += (0x4 * (poor_net_indicator.plus ? 1 : -1));
//     }
//   }
}

void vita_h264_start() {
  active_video_thread = true;
//   vita2d_set_vblank_wait(config.enable_vita_vblank_wait);
}

void vita_h264_stop() {
  // vita2d_set_vblank_wait(true);
  active_video_thread = false;
}

void vitavideo_show_poor_net_indicator() {
  poor_net_indicator.activated = true;
}

void vitavideo_hide_poor_net_indicator() {
  //poor_net_indicator.activated = false;
  memset(&poor_net_indicator, 0, sizeof(indicator_status));
}

int vitavideo_initialized() {
  return video_status != NOT_INIT;
}