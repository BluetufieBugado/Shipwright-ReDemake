#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"

extern "C" {
#include "z64.h"
#include "macros.h"
extern PlayState* gPlayState;
}

// Opção "Remake Controls".
// Padrão = 1 (ligado) para você poder testar antes de existir o checkbox no menu.
// Quando o checkbox existir, ele controla este mesmo valor.
#define CVAR_REMAKE_CONTROLS_NAME CVAR_ENHANCEMENT("RemakeControls")
#define CVAR_REMAKE_CONTROLS_DEFAULT 1
#define CVAR_REMAKE_CONTROLS_VALUE CVarGetInteger(CVAR_REMAKE_CONTROLS_NAME, CVAR_REMAKE_CONTROLS_DEFAULT)

// Roda uma vez por frame, antes do jogo processar o Link.
//
// Etapa 1: segurar Z (que você liga ao ZL no editor de controles do SoH)
// também "aperta" o R do N64, que é o botão do escudo no jogo original.
// Resultado: ZL mira e levanta o escudo ao mesmo tempo, como no remake.
static void OnGameFrameUpdateRemakeControls() {
    if (!GameInteractor::IsSaveLoaded() || gPlayState == nullptr) {
        return;
    }

    Input* input = &gPlayState->state.input[0];

    if (CHECK_BTN_ANY(input->cur.button, BTN_Z)) {
        input->cur.button |= BTN_R;

        // "press" = o frame exato em que o botão foi apertado
        if (CHECK_BTN_ANY(input->press.button, BTN_Z)) {
            input->press.button |= BTN_R;
        }
    }
}

static void RegisterRemakeControls() {
    // Só registra o hook quando a opção está ligada (mesmo padrão do MoonJump.cpp)
    COND_HOOK(OnGameFrameUpdate, CVAR_REMAKE_CONTROLS_VALUE, OnGameFrameUpdateRemakeControls);
}

static RegisterShipInitFunc initFunc(RegisterRemakeControls, { CVAR_REMAKE_CONTROLS_NAME });
