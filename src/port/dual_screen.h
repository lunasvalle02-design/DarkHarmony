// Dual-screen front end for handhelds with a second (touch) display, such as
// the Anbernic RG DS / RG DS Plus on ROCKNIX.
//
// The runtime keeps the game on its own (top) window. This module owns a
// second window — titled "... Bottom Screen", which ROCKNIX's sway rules move
// to the secondary output and power on — and draws there the castle map, the
// player status and the port settings, all touch driven. While the game's own
// pause menu or map screen is open, that image moves to the bottom screen and
// the top screen holds the last gameplay frame, like a Nintendo DS title.
//
// Everything runs on the guest thread through two RunOptions seams:
// present_filter (once per presented frame) and host_event (SDL events).
#pragma once

#include <cstdint>
#include <string>

#include "port_config.h"

namespace port {

struct DualScreenOptions {
    std::string config_path;   // hod.cfg, rewritten when a setting changes
    std::string window_title = "Harmony of Dissonance Bottom Screen";
    bool enabled = true;       // false: no second window (single-screen devices)
};

void dual_screen_init(const DualScreenOptions& options, const PortConfig& config);
const std::uint8_t* dual_screen_present(const std::uint8_t* rgb888, int width, int height);
int dual_screen_event(const void* sdl_event);
void dual_screen_shutdown();

}  // namespace port
