#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
extern PlayState* gPlayState;
}

#define CVAR_REMAKE_CONTROLS_NAME CVAR_ENHANCEMENT("RemakeControls")
#define CVAR_REMAKE_CONTROLS_DEFAULT 1
#define CVAR_REMAKE_CONTROLS_VALUE CVarGetInteger(CVAR_REMAKE_CONTROLS_NAME, CVAR_REMAKE_CONTROLS_DEFAULT)

// Segurar Z também "aperta" o R do N64 (escudo).
static void ApplyShieldInput(Input* input) {
    if (CHECK_BTN_ANY(input->cur.button, BTN_Z)) {
        input->cur.button |= BTN_R;

        if (CHECK_BTN_ANY(input->press.button, BTN_Z)) {
            input->press.button |= BTN_R;
        }
    }
}

// ===== TESTE TEMPORÁRIO (diagnóstico) =====
// Cada vez que você APERTA o ZL, ganha rupias:
//   +1  = o hook do "inicio do frame" está rodando e viu o Z
//   +10 = o hook do "update do Link" está rodando e viu o Z
//   +11 = os dois
//   nada = este arquivo não está sendo executado
static void OnFrameRemakeControls() {
    if (!GameInteractor::IsSaveLoaded() || gPlayState == nullptr) {
        return;
    }
    Input* input = &gPlayState->state.input[0];

    if (CHECK_BTN_ANY(input->press.button, BTN_Z)) {
        gSaveContext.rupeeAccumulator += 1;
    }
    ApplyShieldInput(input);
}

static void OnPlayerUpdateRemakeControls() {
    if (!GameInteractor::IsSaveLoaded() || gPlayState == nullptr) {
        return;
    }
    Input* input = &gPlayState->state.input[0];

    if (CHECK_BTN_ANY(input->press.button, BTN_Z)) {
        gSaveContext.rupeeAccumulator += 10;
    }
    ApplyShieldInput(input);
}

static void RegisterFrameHook() {
    COND_HOOK(OnGameFrameUpdate, CVAR_REMAKE_CONTROLS_VALUE, OnFrameRemakeControls);
}

static void RegisterPlayerHook() {
    COND_HOOK(OnPlayerUpdate, CVAR_REMAKE_CONTROLS_VALUE, OnPlayerUpdateRemakeControls);
}

static RegisterShipInitFunc initFrame(RegisterFrameHook, { CVAR_REMAKE_CONTROLS_NAME });
static RegisterShipInitFunc initPlayer(RegisterPlayerHook, { CVAR_REMAKE_CONTROLS_NAME });
