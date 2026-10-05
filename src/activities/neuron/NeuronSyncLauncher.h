#pragma once

class Activity;
class GfxRenderer;
class MappedInputManager;

// Opens the Neuron sync screen on top of `from`. The sync reboots into the
// Bridge when it closes (WiFi + TLS fragment the heap), so nothing returns.
void launchNeuronSync(Activity& from, GfxRenderer& renderer, MappedInputManager& mappedInput);
