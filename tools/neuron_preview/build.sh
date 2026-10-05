#!/usr/bin/env bash
# Builds and runs the host preview, then renders PNGs into out/.
set -euo pipefail
cd "$(dirname "$0")"
ROOT=../..
python3 gen_stubs.py
python3 make_sample_sd.py
g++ -std=gnu++20 -O1 -w \
  -Istubs -I$ROOT/src -I$ROOT/lib/Memory \
  preview_main.cpp stubs/HalSystemStubs.cpp \
  $ROOT/src/neuron/NeuronData.cpp $ROOT/src/neuron/NeuronClock.cpp $ROOT/src/neuron/NeuronDraw.cpp \
  $ROOT/src/activities/neuron/BridgeActivity.cpp $ROOT/src/activities/neuron/WeatherActivity.cpp \
  $ROOT/src/activities/neuron/MarketsActivity.cpp $ROOT/src/activities/neuron/FocusActivity.cpp \
  $ROOT/src/activities/neuron/NeuronSyncActivity.cpp \
  -o out_preview
NEURON_SD=sd ./out_preview
python3 render.py out/*.ops
