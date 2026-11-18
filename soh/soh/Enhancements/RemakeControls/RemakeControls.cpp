#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/cvar_prefixes.h"

#include "RemakeControls.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
#include "functions.h"
extern PlayState* gPlayState;

// Definidas em z_parameter.c
void Interface_LoadItemIcon1(PlayState* play, u16 button);
void func_80083108(PlayState* play);
s32 Player_InCsMode(PlayState* play);
}

#define CVAR_REMAKE_CONTROLS_NAME CVAR_ENHANCEMENT("RemakeControls")
#define CVAR_REMAKE_CONTROLS_DEFAULT 1
#define CVAR_REMAKE_CONTROLS_VALUE CVarGetInteger(CVAR_REMAKE_CONTROLS_NAME, CVAR_REMAKE_CONTROLS_DEFAULT)

// Ordenação do carrossel: 0 = ordem dos slots do inventário, 1 = agrupado por tipo
#define CVAR_REMAKE_SORT_NAME CVAR_ENHANCEMENT("RemakeControlsSortByType")

// ---- HUD: item em foco no canto da tela (desenhado em RemakeControlsOverlay.cpp) ----
static bool sCornerHidden = false;

extern "C" int RemakeControls_IsEnabled(void) {
    return CVAR_REMAKE_CONTROLS_VALUE;
}

// Com o sistema ligado, os ícones C-esquerdo/C-baixo/C-direito originais deixam de ser desenhados.
extern "C" int RemakeControls_HideOriginalCButtons(void) {
    return CVAR_REMAKE_CONTROLS_VALUE;
}

extern "C" void RemakeControls_SetCornerHidden(int hidden) {
    sCornerHidden = hidden != 0;
}

bool RemakeControls_IsCornerHidden() {
    return sCornerHidden;
}

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

// Posição do item na ordenação "por tipo" (menor = aparece antes). Os números são os ITEM_* do inventário
// (os mesmos de GetItemName no RemakeControlsOverlay.cpp). Para mudar a ordem, é só reordenar esta lista.
static int GetItemTypeRank(int item) {
    static const u8 kTypeOrder[] = {
        // Ataque à distância e ferramentas de combate
        0x03, // Fairy Bow
        0x04, // Fire Arrow
        0x0C, // Ice Arrow
        0x12, // Light Arrow
        0x06, // Fairy Slingshot
        0x0A, // Hookshot
        0x0B, // Longshot
        0x0E, // Boomerang
        0x11, // Megaton Hammer
        // Explosivos e consumíveis
        0x02, // Bomb
        0x09, // Bombchu
        0x00, // Deku Stick
        0x01, // Deku Nut
        0x10, // Magic Beans
        // Magias e utilidades
        0x05, // Din's Fire
        0x0D, // Farore's Wind
        0x13, // Nayru's Love
        0x0F, // Lens of Truth
        // Ocarinas
        0x07, // Fairy Ocarina
        0x08, // Ocarina of Time
        // Garrafas (todos os conteúdos, 0x14 a 0x20, contam como um grupo)
        0x14,
    };
    if (item >= 0x14 && item <= 0x20) {
        item = 0x14;
    }
    for (int i = 0; i < (int)ARRAY_COUNT(kTypeOrder); i++) {
        if (kTypeOrder[i] == item) {
            return i;
        }
    }
    return 100 + item; // o resto (itens de troca e máscaras) vai no fim, pela numeração do item
}

bool RemakeControls_IsSortByType() {
    return CVarGetInteger(CVAR_REMAKE_SORT_NAME, 0) != 0;
}

// Itens que servem para a idade atual, na ordem do carrossel. Em empate, vale a ordem dos slots.
int RemakeControls_GetCarouselSlots(int* outSlots, int maxSlots) {
    int count = 0;
    for (int s = 0; s < (int)ARRAY_COUNT(gSaveContext.inventory.items) && count < maxSlots; s++) {
        if (IsSlotUsableForCurrentAge(s)) {
            outSlots[count++] = s;
        }
    }
    if (RemakeControls_IsSortByType()) {
        // Ordenação por inserção (no máximo 24 itens), estável
        for (int i = 1; i < count; i++) {
            const int slot = outSlots[i];
            const int rank = GetItemTypeRank(gSaveContext.inventory.items[slot]);
            int j = i - 1;
            while (j >= 0 && GetItemTypeRank(gSaveContext.inventory.items[outSlots[j]]) > rank) {
                outSlots[j + 1] = outSlots[j];
                j--;
            }
            outSlots[j + 1] = slot;
        }
    }
    return count;
}

// O item em foco vive no slot C-esquerdo (buttonItems[1] / cButtonSlots[0]).
// Assim o HUD, a munição, o "botão desativado" e todas as regras de uso do jogo funcionam sem mudanças.
// Retorna false se não existe nenhum item válido para a idade atual.
static bool CycleFocusedItem(PlayState* play, int dir) {
    int slots[ARRAY_COUNT(gSaveContext.inventory.items)];
    const int count = RemakeControls_GetCarouselSlots(slots, (int)ARRAY_COUNT(slots));
    if (count == 0) {
        return false;
    }

    // Posição do item em foco na lista; se não está nela, começa de uma ponta
    const int current = gSaveContext.equips.cButtonSlots[0];
    int idx = (dir > 0) ? -1 : count;
    for (int i = 0; i < count; i++) {
        if (slots[i] == current) {
            idx = i;
            break;
        }
    }
    idx = ((idx + dir) % count + count) % count;

    const int slot = slots[idx];
    gSaveContext.equips.cButtonSlots[0] = slot;
    gSaveContext.equips.buttonItems[1] = gSaveContext.inventory.items[slot];
    Interface_LoadItemIcon1(play, 1);
    func_80083108(play); // recalcula botões ativos/desativados para o novo item
    return true;
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

// ---- Carrossel de itens (etapa 1: só a pausa; a faixa de itens na tela vem depois) ----
// Segurar L ou R pausa o mundo. Enquanto está aberto, L/R trocam o item em foco.
// Soltar os dois fecha o carrossel e o jogo volta a correr.
static bool sCarouselOpen = false;

static bool CanUseCarousel(PlayState* play) {
    return gSaveContext.gameMode == GAMEMODE_NORMAL && play->msgCtx.msgMode == MSGMODE_NONE &&
           play->gameOverCtx.state == GAMEOVER_INACTIVE && play->transitionMode == TRANS_MODE_OFF &&
           play->transitionTrigger == TRANS_TRIGGER_OFF && !Player_InCsMode(play);
}

// Som de cursor do menu de pausa, tocado a cada troca de item no carrossel.
static void PlayCarouselCursorSound() {
    Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// Chamada por Play_Update (z_play.c), logo depois de calcular isPaused.
// Retorna 1 se o mundo deve ficar parado neste frame.
extern "C" s32 RemakeControls_UpdateCarousel(PlayState* play, s32 isPaused) {
    if (!CVAR_REMAKE_CONTROLS_VALUE || isPaused || !CanUseCarousel(play)) {
        sCarouselOpen = false;
        return 0;
    }

    // Lê o input físico direto: L/R chegam aqui como C-esquerdo/C-direito.
    const Input* input = &play->state.input[0];
    const bool prevPress = CHECK_BTN_ANY(input->press.button, BTN_CLEFT);
    const bool nextPress = CHECK_BTN_ANY(input->press.button, BTN_CRIGHT);
    const bool held = CHECK_BTN_ANY(input->cur.button, BTN_CLEFT | BTN_CRIGHT);

    // O toque que abre o carrossel só abre: o item em foco continua o mesmo. Os toques seguintes é que trocam.
    bool justOpened = false;
    if (!sCarouselOpen) {
        if (!(prevPress || nextPress)) {
            return 0;
        }
        sCarouselOpen = true;
        justOpened = true;
    }

    if (!held) {
        sCarouselOpen = false;
        return 0;
    }

    // Y (chega como B) alterna a ordenação. O item em foco continua o mesmo, só muda a ordem da faixa.
    if (CHECK_BTN_ANY(input->press.button, BTN_B)) {
        CVarSetInteger(CVAR_REMAKE_SORT_NAME, RemakeControls_IsSortByType() ? 0 : 1);
        PlayCarouselCursorSound();
    }

    if (justOpened) {
        return 1;
    }

    if (prevPress) {
        if (CycleFocusedItem(play, -1)) {
            PlayCarouselCursorSound();
        }
    } else if (nextPress) {
        if (CycleFocusedItem(play, 1)) {
            PlayCarouselCursorSound();
        }
    }
    return 1;
}

bool RemakeControls_IsCarouselOpen() {
    return sCarouselOpen;
}

bool RemakeControls_IsSlotUsable(int slot) {
    return IsSlotUsableForCurrentAge(slot);
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

    // Consome os portadores: o jogo não pode reagir a eles como botões C de verdade.
    input->cur.button &= ~kRemakeCarrierButtons;
    input->press.button &= ~kRemakeCarrierButtons;

    ValidateFocusedItem(gPlayState);

    // ZR usa o item em foco: o jogo enxerga um C-esquerdo comum.
    if (useHeld) {
        input->cur.button |= BTN_CLEFT;
    }
    if (usePress) {
        input->press.button |= BTN_CLEFT;
    }
}
