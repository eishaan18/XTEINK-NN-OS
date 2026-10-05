#!/usr/bin/env bash
# Host checks for the Neuron data, formatting and network-parsing code.
set -euo pipefail
cd "$(dirname "$0")"
ROOT=../..
JSON=${ARDUINOJSON_SRC:-$ROOT/.pio/libdeps/gh_release/ArduinoJson/src}
[ -d "$JSON" ] || JSON=$(ls -d $ROOT/.pio/libdeps/*/ArduinoJson/src | head -1)
python3 gen_stubs.py >/dev/null
g++ -std=gnu++20 -O0 -g -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined \
  -Iteststubs -Istubs -I$ROOT/src -I$ROOT/lib/Memory -I"$JSON" \
  test_logic.cpp stubs/HalSystemStubs.cpp \
  $ROOT/src/neuron/NeuronData.cpp $ROOT/src/neuron/NeuronDraw.cpp $ROOT/src/neuron/NeuronNet.cpp \
  -o out_tests
SD_DIR=$(mktemp -d)
trap 'rm -rf "$SD_DIR"' EXIT
NEURON_SD="$SD_DIR" ./out_tests
