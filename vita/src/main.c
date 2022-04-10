/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015, 2016 Iwan Timmer
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
#include <ctype.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <psp2/ctrl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/rng.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/rtc.h>
#include <psp2/sysmodule.h>
#include <psp2/touch.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <debugnet.h>

#include "context.h"
#include "discovery.h"
#include "ui.h"

static void vita_init() {
  // Seed OpenSSL with Sony-grade random number generator
  char random_seed[0x40] = {0};
  sceKernelGetRandomNumber(random_seed, sizeof(random_seed));
  RAND_seed(random_seed, sizeof(random_seed));
  OpenSSL_add_all_algorithms();

  printf("Vita Chiaki %s\n", CHIAKI_VERSION);

  int ret = 0;

  // Initialize networking stack with a 100KiB buffer
  ret = sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
  size_t net_mem_sz = 100 * 1024;
  SceNetInitParam net_param = {0};
  net_param.memory = calloc(net_mem_sz, 1);
  net_param.size = net_mem_sz;
  ret = sceNetInit(&net_param);
  
  // Initialize touch sampling
	sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, 1);
}

int main(int argc, char* argv[]) {
  debugNetInit("192.168.2.49", 31338, DEBUG);
  vita_init();

  // TODO: initialize power control thread
  // if (!vitapower_init()) {
  //   printf("Failed to init power!");
  //   loop_forever();
  // }

  // TODO: initialize input thread
  // if (!vitainput_init()) {
  //   printf("Failed to init input!");
  //   loop_forever();
  // }

  sceIoMkdir("ux0:/data/vita-chiaki", 0777);

  // TODO: configure power control
  // vitapower_config(config);
  // TODO: configure input handling
  // vitainput_config(config);

  vita_chiaki_init_context();
  if (context.config.auto_discovery) {
    LOGD("Starting discovery");
    ChiakiErrorCode err = start_discovery(NULL, NULL);
    if (err != CHIAKI_ERR_SUCCESS) {
      LOGD("Failed to start discovery: %d\n", err);
      sceKernelExitProcess(err);
    }
  }

  LOGD("Starting to draw UI");
  draw_ui();

  // TODO: Cleanup
}
