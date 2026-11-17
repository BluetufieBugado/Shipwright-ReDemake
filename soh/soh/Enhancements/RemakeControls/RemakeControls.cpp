#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/cvar_prefixes.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
extern PlayState* gPlayState;

// Definidas em z_parameter.c
void Interface_LoadItemIcon1(PlayState* play, u16 button);
void func_80083108(PlayState* play);
}

#define CVAR_REMAKE_CONTROLS_NAME CVAR_ENHANCEMENT("RemakeControls")
#define CVAR_REMAKE_CONTROLS_DEFAULT 1
#define CVAR_REMAKE_CONTROLS_VALUE CVarGetInteger(CVAR_REMAKE_CONTROLS_NAME, CVAR_REMAKE_CONTROLS_DEFAULT)

// Botões N64 usados como "portadores" (ver tabela do resumo):
//   ZR -> C-cima    : usar item em foco
//   L  -> C-esquerdo: item anterior
//   R  -> C-direito : próximo item
//   X  -> C-baixo   : pulo (ainda não implementado, só é consumido)
static const u16 kRemakeCarrierButtons = BTN_CUP | BTN_CLEFT | BTN_CRIGHT | BTN_CDOWN;

#ifndef AGE_REQ_NONE
#define AGE_REQ_NONE 9
#endif

// O item do slot serve para a idade atual do Link? Usa as mesmas tabelas do menu de pausa
// (gSlotAgeReqs e gItemAgeReqs), lidas ao vivo porque o SoH as altera em z_play.c.
static bool IsSlotUsableForCurrentAge(int slot) {
    const u8 item = gSaveContext.inventory.items[slot];
    if (item == ITEM_NONE) {
        return false;
    }
    const int slotReq = gSlotAgeReqs[slot];
    const int itemReq = gItemAgeReqs[item];
    return (slotReq == AGE_REQ_NONE || slotReq == gSaveContext.linkAge) &&
           (itemReq == AGE_REQ_NONE || itemReq == gSaveContext.linkAge);
}

// O item em foco vive no slot C-esquerdo (buttonItems[1] / cButtonSlots[0]).
// Assim o HUD, a munição, o "botão desativado" e todas as regras de uso do jogo funcionam sem mudanças.
// Retorna false se não existe nenhum item válido para a idade atual.
static bool CycleFocusedItem(PlayState* play, int dir) {
    const int count = ARRAY_COUNT(gSaveContext.inventory.items);
    int slot = gSaveContext.equips.cButtonSlots[0];

    if (slot >= count) {
        slot = (dir > 0) ? -1 : count; // nada equipado: começa de uma ponta
    }

    for (int n = 0; n < count; n++) {
        slot = (slot + dir + count) % count;
        if (IsSlotUsableForCurrentAge(slot)) {
            gSaveContext.equips.cButtonSlots[0] = slot;
            gSaveContext.equips.buttonItems[1] = gSaveContext.inventory.items[slot];
            Interface_LoadItemIcon1(play, 1);
            func_80083108(play); // recalcula botões ativos/desativados para o novo item
            return true;
        }
    }
    return false;
}

// Se o item em foco deixou de servir (mudou de idade, save antigo), passa para o próximo válido.
static void ValidateFocusedItem(PlayState* play) {
    const int slot = gSaveContext.equips.cButtonSlots[0];
    if (slot >= ARRAY_COUNT(gSaveContext.inventory.items) || IsSlotUsableForCurrentAge(slot)) {
        return; // nada em foco, ou o item em foco é válido
    }
    if (!CycleFocusedItem(play, 1)) {
        // Nenhum item válido: esvazia o foco (sem carregar ícone, o HUD esconde o slot)
        gSaveContext.equips.cButtonSlots[0] = ITEM_NONE;
        gSaveContext.equips.buttonItems[1] = ITEM_NONE;
        func_80083108(play);
    }
}

// Chamada por Player_Update (z_player.c) na cópia local do input do Link.
extern "C" void RemakeControls_ModifyPlayerInput(Input* input) {
    if (!CVAR_REMAKE_CONTROLS_VALUE) {
        return;
    }

    // ZL (Z) mira e também levanta o escudo, como Z+R no original.
    if (CHECK_BTN_ANY(input->cur.button, BTN_Z)) {
        input->cur.button |= BTN_R;
    }
    if (CHECK_BTN_ANY(input->press.button, BTN_Z)) {
        input->press.button |= BTN_R;
    }

    if (gPlayState == nullptr) {
        return;
    }

    const bool useHeld = CHECK_BTN_ANY(input->cur.button, BTN_CUP);
    const bool usePress = CHECK_BTN_ANY(input->press.button, BTN_CUP);
    const bool prevPress = CHECK_BTN_ANY(input->press.button, BTN_CLEFT);
    const bool nextPress = CHECK_BTN_ANY(input->press.button, BTN_CRIGHT);

    // Consome os portadores: o jogo não pode reagir a eles como botões C de verdade.
    input->cur.button &= ~kRemakeCarrierButtons;
    input->press.button &= ~kRemakeCarrierButtons;

    ValidateFocusedItem(gPlayState);

    if (prevPress) {
        CycleFocusedItem(gPlayState, -1);
    } else if (nextPress) {
        CycleFocusedItem(gPlayState, 1);
    }

    // ZR usa o item em foco: o jogo enxerga um C-esquerdo comum.
    if (useHeld) {
        input->cur.button |= BTN_CLEFT;
    }
    if (usePress) {
        input->press.button |= BTN_CLEFT;
    }
}
