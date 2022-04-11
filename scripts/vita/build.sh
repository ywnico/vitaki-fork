#!/bin/bash

set -veo pipefail

SCRIPTDIR=$(dirname "$0")
BASEDIR=$(realpath "${SCRIPTDIR}/../../")

build_chiaki (){
	pushd "${BASEDIR}"
	# if [[ ! -d './build' ]]; then
		cmake -B "./build" \
			-DCMAKE_BUILD_TYPE=Debug \
			-DCMAKE_TOOLCHAIN_FILE=${VITASDK}/share/vita.toolchain.cmake \
			-DCHIAKI_ENABLE_VITA=ON \
			-DCHIAKI_ENABLE_TESTS=OFF \
			-DCHIAKI_ENABLE_CLI=OFF \
			-DCHIAKI_ENABLE_GUI=OFF \
			-DCHIAKI_ENABLE_ANDROID=OFF \
			-DCHIAKI_ENABLE_SETSU=OFF \
			-DCHIAKI_FFMPEG_DEFAULT=OFF \
			-DCHIAKI_ENABLE_FFMPEG_DECODER=OFF \
			-DCHIAKI_ENABLE_PI_DECODER=OFF \
			-DCHIAKI_USE_SYSTEM_NANOPB=OFF \
			-DCHIAKI_LIB_ENABLE_OPUS=OFF
	# fi
	make -j$(nproc) -C "./build"
	python3 ./scripts/vita/devtool.py --upload-assets --host $PSVITAIP upload
	popd
}

build_chiaki
