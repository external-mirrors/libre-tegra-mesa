#!/bin/bash

BUILD_DIR="${PWD}/build"
INSTALL_DIR="${BUILD_DIR}/install_dir"

EXTRA_SETUP_ARGS=""
if [[ -f "/sys/devices/soc0/family" ]] && [[ "$(cat /sys/devices/soc0/family)" = "Tegra" ]]; then
	echo "Tegra! skipping drm-shim";

	# gnu2 tlsdesc is broken in binutils
	export CFLAGS="$CFLAGS -mtls-dialect=gnu"
	export CXXFLAGS="$CXXFLAGS -mtls-dialect=gnu"
else
	EXTRA_SETUP_ARGS="${EXTRA_SETUP_ARGS} -Dtools=drm-shim"
fi
