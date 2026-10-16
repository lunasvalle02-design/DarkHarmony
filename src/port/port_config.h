// Player settings of the handheld port, stored as key=value lines in
// <port dir>/hod.cfg. Unknown keys are ignored; missing keys keep defaults.
#pragma once

#include <string>

namespace port {

enum class TopScale : int { Fit = 0, Integer = 1, Stretch = 2 };
enum class Language : int { Spanish = 0, English = 1 };

struct PortConfig {
    TopScale top_scale = TopScale::Integer;
    bool smooth_filter = false;
    int volume = 100;               // 0..100
    bool menus_on_bottom = true;    // pause menu / map screen go to the bottom screen
    bool swap_ab = false;
    bool resume_after_sleep = true; // suspend state if the battery dies asleep
    bool show_fps = false;
    int map_zoom = 1;               // 1 whole castle, 2 follow the player
    Language language = Language::Spanish;

    bool load(const std::string& path);
    bool save(const std::string& path) const;
};

}  // namespace port
