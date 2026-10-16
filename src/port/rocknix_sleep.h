// Suspend integration for ROCKNIX (systemd-logind), shared with the Zelda3 DS
// port: a logind "delay" inhibitor gives the game a moment to write its save
// before games.slice is frozen. libdbus-1 is dlopen'ed, so the binary has no
// hard dependency on it; without D-Bus or logind the module stays off.
#pragma once

namespace port {

enum class SleepEvent { None, GoingToSleep, Resumed };

bool sleep_init();
// Non-blocking; call once per frame.
SleepEvent sleep_poll();
// Release the inhibitor: logind suspends once every delay lock is gone.
void sleep_ready();
void sleep_shutdown();

}  // namespace port
