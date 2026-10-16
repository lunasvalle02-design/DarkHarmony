#include "port_config.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

namespace port {
namespace {

std::string trim(std::string s) {
    const char* ws = " \t\r\n";
    s.erase(0, s.find_first_not_of(ws));
    s.erase(s.find_last_not_of(ws) + 1);
    return s;
}

bool as_bool(const std::string& v) { return v == "1" || v == "true" || v == "yes" || v == "on"; }

}  // namespace

bool PortConfig::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));
        const int n = std::atoi(value.c_str());
        if (key == "top_scale") {
            if (value == "fit") top_scale = TopScale::Fit;
            else if (value == "stretch") top_scale = TopScale::Stretch;
            else top_scale = TopScale::Integer;
        } else if (key == "smooth_filter") smooth_filter = as_bool(value);
        else if (key == "volume") volume = std::clamp(n, 0, 100);
        else if (key == "menus_on_bottom") menus_on_bottom = as_bool(value);
        else if (key == "swap_ab") swap_ab = as_bool(value);
        else if (key == "resume_after_sleep") resume_after_sleep = as_bool(value);
        else if (key == "show_fps") show_fps = as_bool(value);
        else if (key == "map_zoom") map_zoom = std::clamp(n, 1, 2);
        else if (key == "language") language = value == "en" ? Language::English : Language::Spanish;
    }
    return true;
}

bool PortConfig::save(const std::string& path) const {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) return false;
        static const char* scales[] = {"fit", "integer", "stretch"};
        out << "# Castlevania: Harmony of Dissonance — dual-screen port settings.\n"
            << "# Edited from the bottom screen; safe to change by hand while the game is closed.\n"
            << "top_scale=" << scales[static_cast<int>(top_scale)] << "\n"
            << "smooth_filter=" << (smooth_filter ? 1 : 0) << "\n"
            << "volume=" << volume << "\n"
            << "menus_on_bottom=" << (menus_on_bottom ? 1 : 0) << "\n"
            << "swap_ab=" << (swap_ab ? 1 : 0) << "\n"
            << "resume_after_sleep=" << (resume_after_sleep ? 1 : 0) << "\n"
            << "show_fps=" << (show_fps ? 1 : 0) << "\n"
            << "map_zoom=" << map_zoom << "\n"
            << "language=" << (language == Language::English ? "en" : "es") << "\n";
        if (!out) return false;
    }
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

}  // namespace port
