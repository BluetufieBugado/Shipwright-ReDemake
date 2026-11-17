#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/cvar_prefixes.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
}

#define CVAR_REMAKE_CONTROLS_NAME CVAR_ENHANCEMENT("RemakeControls")
#define CVAR_REMAKE_CONTROLS_DEFAULT 1
#define CVAR_REMAKE_CONTROLS_VALUE CVarGetInteger(CVAR_REMAKE_CONTROLS_NAME, CVAR_REMAKE_CONTROLS_DEFAULT)

// Chamada por Player_Update (z_player.c) logo depois da cópia local do input do Link.
// ZL (ligado ao Z do N64) mira e também levanta o escudo, como Z+R no original.
extern "C" void RemakeControls_ModifyPlayerInput(Input* input) {
    if (!CVAR_REMAKE_CONTROLS_VALUE) {
        return;
    }

    if (CHECK_BTN_ANY(input->cur.button, BTN_Z)) {
        input->cur.button |= BTN_R;
    }
    if (CHECK_BTN_ANY(input->press.button, BTN_Z)) {
        input->press.button |= BTN_R;
    }
}
