#include "port_strings.h"

namespace port {
namespace {

struct Pair {
    const char* es;
    const char* en;
};

constexpr Pair kStrings[] = {
    {"MAPA", "MAP"},                        // TabMap
    {"ESTADO", "STATUS"},                   // TabStatus
    {"AJUSTES", "SETTINGS"},                // TabSettings
    {"CASTILLO A", "CASTLE A"},             // CastleA
    {"CASTILLO B", "CASTLE B"},             // CastleB
    {"CASTILLOS A+B", "CASTLES A+B"},       // BothCastles
    {"MAPA", "MAP"},                        // MapTitle
    {"ESTADO", "STATUS"},                   // Status
    {"NIVEL", "LEVEL"},                     // Level
    {"EXP", "EXP"},                         // Exp
    {"ORO", "GOLD"},                        // Gold
    {"TIEMPO", "TIME"},                     // Time
    {"LIBROS", "BOOKS"},                    // Books
    {"RELIQUIAS", "RELICS"},                // Relics
    {"MAPA", "MAP"},                        // MapPercent
    {"PANTALLA SUPERIOR", "TOP SCREEN"},    // TopScreen
    {"AJUSTADA", "FIT"},                    // ScaleFit
    {"ENTERA", "INTEGER"},                  // ScaleInteger
    {"ESTIRADA", "STRETCH"},                // ScaleStretch
    {"FILTRO", "FILTER"},                   // Filter
    {"NÍTIDO", "SHARP"},                    // Sharp
    {"SUAVE", "SMOOTH"},                    // Smooth
    {"VOLUMEN", "VOLUME"},                  // Volume
    {"MENÚS ABAJO", "MENUS BELOW"},         // MenusBelow
    {"INTERCAMBIAR A/B", "SWAP A/B"},       // SwapAB
    {"TURBO", "TURBO"},                     // Turbo
    {"MOSTRAR FPS", "SHOW FPS"},            // ShowFps
    {"REANUDAR TRAS SUSPENDER", "RESUME AFTER SLEEP"},  // ResumeAfterSleep
    {"SALTO DE FRAMES", "FRAMESKIP"},       // FrameSkip
    {"AUTO", "AUTO"},                       // Auto
    {"CALIBRAR TÁCTIL", "CALIBRATE TOUCH"},   // CalibrateTouch
    {"TOCA EL CENTRO DE LA CRUZ", "TAP THE CENTRE OF THE CROSS"},  // CalibrateHint
    {"TÁCTIL CALIBRADO", "TOUCH CALIBRATED"}, // TouchCalibrated
    {"GUARDAR ESTADO", "SAVE STATE"},       // SaveState
    {"CARGAR ESTADO", "LOAD STATE"},        // LoadState
    {"IDIOMA", "LANGUAGE"},                 // Language
    {"ESPAÑOL", "ENGLISH"},                 // LanguageName
    {"SALIR DEL JUEGO", "QUIT GAME"},       // Quit
    {"SÍ", "ON"},                           // Yes
    {"NO", "OFF"},                          // No
    {"TOCA OTRA VEZ", "TAP AGAIN"},         // TapAgain
    {"ESTADO GUARDADO", "STATE SAVED"},     // StateSaved
    {"ESTADO CARGADO", "STATE LOADED"},     // StateLoaded
    {"GUARDANDO...", "SAVING..."},          // Saving
    {"MENÚ DE PAUSA", "PAUSE MENU"},        // PauseMenu
    {"MAPA DEL JUEGO", "GAME MAP"},         // GameMap
    {"EN LA PANTALLA INFERIOR", "ON THE BOTTOM SCREEN"},  // OnBottomScreen
    {"START / SELECT: VOLVER", "START / SELECT: BACK"},   // CloseHint
    {"EL MAPA APARECE AL ENTRAR EN EL CASTILLO", "THE MAP APPEARS INSIDE THE CASTLE"},  // NotInCastle
    {"TOCA EL MAPA: ZOOM", "TAP THE MAP: ZOOM"},          // ZoomHint
};
static_assert(sizeof(kStrings) / sizeof(kStrings[0]) == static_cast<int>(Str::Count),
              "every Str needs a translation");

}  // namespace

const char* tr(Str id, Language lang) {
    const Pair& p = kStrings[static_cast<int>(id)];
    return lang == Language::English ? p.en : p.es;
}

}  // namespace port
