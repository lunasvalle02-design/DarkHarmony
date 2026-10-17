// Bottom-screen texts (Spanish / English).
#pragma once

#include "port_config.h"

namespace port {

enum class Str : int {
    TabMap, TabStatus, TabSettings,
    CastleA, CastleB, BothCastles, MapTitle,
    Status, Level, Exp, Gold, Time, Books, Relics, MapPercent,
    TopScreen, ScaleFit, ScaleInteger, ScaleStretch,
    Filter, Sharp, Smooth,
    Volume, MenusBelow, SwapAB, Turbo, ShowFps, ResumeAfterSleep,
    FrameSkip, Auto,
    SaveState, LoadState, Language, LanguageName, Quit,
    Yes, No, TapAgain, StateSaved, StateLoaded, Saving,
    PauseMenu, GameMap, OnBottomScreen, CloseHint, NotInCastle, ZoomHint,
    Count
};

const char* tr(Str id, Language lang);

}  // namespace port
