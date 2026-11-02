#!/bin/bash
# Build the headless LVGL render test: real LVGL 9.3 + real radargauge.h
set -e
cd "$(dirname "$0")/.."
LVGL=~/Arduino/libraries/lvgl/src
OUT=host_test/build
# same copy-common step as the Makefile (arduino-cli builds a copy of the
# sketch; here we emulate it by copying into radar/ and adding -Iradar)
rm -rf radar/common && cp -r common radar/common
mkdir -p "$OUT"
# compile LVGL C sources once (cached)
if [ ! -f "$OUT/lvgl_done" ]; then
  echo "compiling LVGL for host..."
  find "$LVGL" -name '*.c' > "$OUT/lvgl_srcs.txt"
  while read -r src; do
    obj="$OUT/$(echo "$src" | md5sum | cut -c1-12).o"
    [ -f "$obj" ] || gcc -O1 -w -I"$LVGL" -c "$src" -o "$obj"
  done < "$OUT/lvgl_srcs.txt"
  touch "$OUT/lvgl_done"
fi
g++ -std=c++17 -O1 -w -I"$LVGL" -Ihost_test/stubs -Iradar \
    host_test/render_test.cpp "$OUT"/*.o -lm -o host_test/render_test -pthread
echo "built host_test/render_test"
