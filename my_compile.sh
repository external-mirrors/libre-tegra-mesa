#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

source ./my_common.sh

time meson compile -C $BUILD_DIR

time meson install -C $BUILD_DIR
