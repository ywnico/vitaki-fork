include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "chiaki")
set(VITA_TITLEID  "CHIAKI001")
# More than two dots and the VPK won't install, so add the patch version without a dot
set(VITA_VERSION "${CHIAKI_VERSION_MAJOR}.${CHIAKI_VERSION_MINOR}${CHIAKI_VERSION_PATCH}")

add_executable(${VITA_APP_NAME}.elf
    src/config.c
    src/context.c
    src/discovery.c
    src/main.c
    src/ui.c

    # src/lib/ime.c
)


target_compile_definitions(${VITA_APP_NAME}.elf PUBLIC
    CHIAKI_VERSION="${CHIAKI_VERSION}"
)

target_include_directories(${VITA_APP_NAME}.elf PRIVATE
    include
)


target_link_libraries(${VITA_APP_NAME}.elf
  # TODO: Do we really need all of those?
  chiaki-lib
  ssl
  crypto
  opus

  SceDisplay_stub
  SceGxm_stub
  SceSysmodule_stub
  SceCtrl_stub
  ScePgf_stub
  ScePvf_stub
  SceCommonDialog_stub
  SceAppMgr_stub

  SceTouch_stub
  SceNet_stub
  SceNetCtl_stub
  ScePower_stub
  SceVideodec_stub
  SceAudio_stub
  SceIme_stub
  SceSsl_stub

  vita2d
  freetype
  png
  jpeg
  z
  m
  c

  debugnet
  pthread
  zip
)


if(CHIAKI_IS_VITA)
    vita_create_self(${VITA_APP_NAME}.self ${VITA_APP_NAME}.elf)

    vita_create_vpk(${VITA_APP_NAME}.vpk ${VITA_TITLEID} ${VITA_APP_NAME}.self
        VERSION ${VITA_VERSION}
        NAME ${VITA_APP_NAME}
        # Livearea stuff
        FILE res/icon0.png sce_sys/icon0.png
        FILE res/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
        FILE res/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
        FILE res/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
        # Textures, other assets
        FILE res/assets/btn_add_active.png assets/btn_add_active.png
        FILE res/assets/btn_add.png assets/btn_add.png
        FILE res/assets/btn_discovery_active.png assets/btn_discovery_active.png
        FILE res/assets/btn_discovery_off_active.png assets/btn_discovery_off_active.png
        FILE res/assets/btn_discovery_off.png assets/btn_discovery_off.png
        FILE res/assets/btn_discovery.png assets/btn_discovery.png
        FILE res/assets/btn_register_active.png assets/btn_register_active.png
        FILE res/assets/btn_register.png assets/btn_register.png
        FILE res/assets/btn_settings_active.png assets/btn_settings_active.png
        FILE res/assets/btn_settings.png assets/btn_settings.png
        FILE res/assets/discovered_host.png assets/discovered_host.png
        FILE res/assets/header_logo.png assets/header_logo.png
        FILE res/assets/ps4_off.png assets/ps4_off.png
        FILE res/assets/ps4.png assets/ps4.png
        FILE res/assets/ps4_rest.png assets/ps4_rest.png
        FILE res/assets/ps5_off.png assets/ps5_off.png
        FILE res/assets/ps5.png assets/ps5.png
        FILE res/assets/ps5_rest.png assets/ps5_rest.png
    )
endif()