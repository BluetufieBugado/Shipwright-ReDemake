#include "z_kaleido_scope.h"
#include "textures/parameter_static/parameter_static.h"
#include "textures/icon_item_static/icon_item_static.h"
#include "soh/Enhancements/randomizer/ShuffleTradeItems.h"
#include "soh/Enhancements/randomizer/RocsFeatherCycle.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/cosmetics/cosmeticsTypes.h"
#include "soh/OTRGlobals.h"

#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include <libultraship/bridge/consolevariablebridge.h>

u8 gAmmoItems[] = {
    ITEM_STICK,   ITEM_NUT,  ITEM_BOMB, ITEM_BOW,  ITEM_NONE, ITEM_NONE, ITEM_SLINGSHOT, ITEM_NONE,
    ITEM_BOMBCHU, ITEM_NONE, ITEM_NONE, ITEM_NONE, ITEM_NONE, ITEM_NONE, ITEM_BEAN,      ITEM_NONE,
};

static s16 sEquipState = 0;
static s16 sEquipAnimTimer = 0;
static s16 sEquipMoveTimer = 10;

static s16 sAmmoVtxOffset[] = {
    0, 2, 4, 6, 99, 99, 8, 99, 10, 99, 99, 99, 99, 99, 12,
};

static s16 sAllAmmoVtxOffset[] = {
    0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46,
};

extern const char* _gAmmoDigit0Tex[];

s8 ItemInSlotUsesAmmo(s16 slot) {
    s16 item = gSaveContext.inventory.items[slot];
    return GameInteractor_Should(VB_DRAW_AMMO_COUNT,
                                 item == ITEM_STICK || item == ITEM_NUT || item == ITEM_BOMB || item == ITEM_BOW ||
                                     item == ITEM_SLINGSHOT || item == ITEM_BOMBCHU || item == ITEM_BEAN,
                                 &item);
}

void KaleidoScope_DrawAmmoCount(PauseContext* pauseCtx, GraphicsContext* gfxCtx, s16 item, int slot) {
    if (!GameInteractor_Should(VB_DRAW_AMMO_COUNT, true, &item)) {
        return;
    }

    s16 ammo;
    s16 i;

    OPEN_DISPS(gfxCtx);

    if (GameInteractor_Should(VB_OVERRIDE_AMMO_COUNT, true, &item, &ammo)) {
        ammo = AMMO(item);
    }

    gDPPipeSync(POLY_OPA_DISP++);

    if (!CHECK_AGE_REQ_SLOT(SLOT(item))) {
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 100, 100, 100, pauseCtx->alpha);
    } else {
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);

        if (ammo == 0) {
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 130, 130, 130, pauseCtx->alpha);
        } else if ((item == ITEM_BOMB && AMMO(item) == CUR_CAPACITY(UPG_BOMB_BAG)) ||
                   (item == ITEM_BOW && AMMO(item) == CUR_CAPACITY(UPG_QUIVER)) ||
                   (item == ITEM_SLINGSHOT && AMMO(item) == CUR_CAPACITY(UPG_BULLET_BAG)) ||
                   (item == ITEM_STICK && AMMO(item) == CUR_CAPACITY(UPG_STICKS)) ||
                   (item == ITEM_NUT && AMMO(item) == CUR_CAPACITY(UPG_NUTS)) || (item == ITEM_BOMBCHU && ammo == 50) ||
                   (item == ITEM_BEAN && ammo == 15) || GameInteractor_Should(VB_COLOR_AMMO_GREEN, false, item)) {
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 120, 255, 0, pauseCtx->alpha);
        }
    }

    for (i = 0; ammo >= 10; i++) {
        ammo -= 10;
    }

    gDPPipeSync(POLY_OPA_DISP++);

    if (i != 0) {
        gSPVertex(
            POLY_OPA_DISP++,
            &pauseCtx->itemVtx[((CVarGetInteger(CVAR_ENHANCEMENT("BetterAmmoRendering"), 0) ? sAllAmmoVtxOffset[slot]
                                                                                            : sAmmoVtxOffset[slot]) +
                                31) *
                               4],
            4, 0);

        gDPLoadTextureBlock(POLY_OPA_DISP++, ((u8*)_gAmmoDigit0Tex[i]), G_IM_FMT_IA, G_IM_SIZ_8b, 8, 8, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                            G_TX_NOLOD);

        gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
    }

    gSPVertex(POLY_OPA_DISP++,
              &pauseCtx->itemVtx[((CVarGetInteger(CVAR_ENHANCEMENT("BetterAmmoRendering"), 0) ? sAllAmmoVtxOffset[slot]
                                                                                              : sAmmoVtxOffset[slot]) +
                                  32) *
                                 4],
              4, 0);

    gDPLoadTextureBlock(POLY_OPA_DISP++, ((u8*)_gAmmoDigit0Tex[ammo]), G_IM_FMT_IA, G_IM_SIZ_8b, 8, 8, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);

    gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);

    CLOSE_DISPS(gfxCtx);
}

void KaleidoScope_SetCursorVtx(PauseContext* pauseCtx, u16 index, Vtx* vtx) {
    pauseCtx->cursorVtx[0].v.ob[0] = vtx[index].v.ob[0];
    pauseCtx->cursorVtx[0].v.ob[1] = vtx[index].v.ob[1];
    KaleidoScope_UpdateCursorSize(pauseCtx); // OTRTODO Why is this needed?
}

void KaleidoScope_SetItemCursorVtx(PauseContext* pauseCtx) {
    KaleidoScope_SetCursorVtx(pauseCtx, pauseCtx->cursorSlot[PAUSE_ITEM] * 4, pauseCtx->itemVtx);
}

#pragma region Item Cycling

s8 gCurrentItemCyclingSlot;

// Vertices for the extra items
static Vtx sCycleExtraItemVtx[] = {
    // Left Item
    VTX(-48, 16, 0, 0 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(-16, 16, 0, 32 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(-48, -16, 0, 0 << 5, 32 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(-16, -16, 0, 32 << 5, 32 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    // Right Item
    VTX(16, 16, 0, 0 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(48, 16, 0, 32 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(16, -16, 0, 0 << 5, 32 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(48, -16, 0, 32 << 5, 32 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
};

// Vertices for the circle behind the items
static Vtx sCycleCircleVtx[] = {
    // Left Item
    VTX(-56, 24, 0, 0 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(-8, 24, 0, 48 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(-56, -24, 0, 0 << 5, 48 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(-8, -24, 0, 48 << 5, 48 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    // Right Item
    VTX(8, 24, 0, 0 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(56, 24, 0, 48 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(8, -24, 0, 0 << 5, 48 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(56, -24, 0, 48 << 5, 48 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
};

// Vertices for A button indicator (coordinates 1.5x larger than texture size)
static Vtx sCycleAButtonVtx[] = {
    VTX(-18, 12, 0, 0 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(18, 12, 0, 24 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(-18, -12, 0, 0 << 5, 16 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(18, -12, 0, 24 << 5, 16 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
};

// Track animation timers for each inventory slot
static int sSlotCycleActiveAnimTimer[24] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

// Renders a left and/or right item for any item slot that can support cycling
void KaleidoScope_DrawItemCycleExtras(PlayState* play, u8 slot, u8 canCycle, u8 leftItem, u8 rightItem) {
    PauseContext* pauseCtx = &play->pauseCtx;

    u8 isCycling = gCurrentItemCyclingSlot == slot;

    OPEN_DISPS(play->state.gfxCtx);

    // Update active cycling animation timer
    if (isCycling) {
        if (sSlotCycleActiveAnimTimer[slot] < 5) {
            sSlotCycleActiveAnimTimer[slot]++;
        }
    } else {
        if (sSlotCycleActiveAnimTimer[slot] > 0) {
            sSlotCycleActiveAnimTimer[slot]--;
        }
    }

    u8 slotItem = gSaveContext.inventory.items[slot];
    u8 showLeftItem = leftItem != ITEM_NONE && slotItem != leftItem;
    u8 showRightItem = rightItem != ITEM_NONE && slotItem != rightItem && leftItem != rightItem;

    // Render the extra cycle items if at least the left or right item are valid
    if (canCycle && slotItem != ITEM_NONE && (showLeftItem || showRightItem)) {
        Matrix_Push();

        Vtx* itemTopLeft = &pauseCtx->itemVtx[slot * 4];
        Vtx* itemBottomRight = &itemTopLeft[3];

        s16 halfX = (itemBottomRight->v.ob[0] - itemTopLeft->v.ob[0]) / 2;
        s16 halfY = (itemBottomRight->v.ob[1] - itemTopLeft->v.ob[1]) / 2;

        Matrix_Translate(itemTopLeft->v.ob[0] + halfX, itemTopLeft->v.ob[1] + halfY, 0, MTXMODE_APPLY);

        f32 animScale = (f32)(5 - sSlotCycleActiveAnimTimer[slot]) / 5;

        // When not cycling or actively animating, shrink and move the items under the main slot item
        if (!isCycling || sSlotCycleActiveAnimTimer[slot] < 5) {
            f32 finalScale = 1.0f - (0.675f * animScale);
            Matrix_Translate(0, -15.0f * animScale, 0, MTXMODE_APPLY);
            Matrix_Scale(finalScale, finalScale, 1.0f, MTXMODE_APPLY);
        }

        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

        // Render A button indicator when hovered and not cycling
        if (!isCycling && sSlotCycleActiveAnimTimer[slot] == 0 && pauseCtx->cursorSlot[PAUSE_ITEM] == slot &&
            pauseCtx->cursorSpecialPos == 0) {
            Color_RGB8 aButtonColor = { 0, 100, 255 };
            if (CVarGetInteger(CVAR_COSMETIC("HUD.AButton.Changed"), 0)) {
                aButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.AButton.Value"), aButtonColor);
            } else if (CVarGetInteger(CVAR_COSMETIC("DefaultColorScheme"), COLORSCHEME_N64) == COLORSCHEME_GAMECUBE) {
                aButtonColor = (Color_RGB8){ 0, 255, 100 };
            }

            gSPVertex(POLY_OPA_DISP++, sCycleAButtonVtx, 4, 0);
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, aButtonColor.r, aButtonColor.g, aButtonColor.b, pauseCtx->alpha);
            gDPLoadTextureBlock(POLY_OPA_DISP++, gABtnSymbolTex, G_IM_FMT_IA, G_IM_SIZ_8b, 24, 16, 0,
                                G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, 4, 4, G_TX_NOLOD, G_TX_NOLOD);
            gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
        }

        // Render a dark circle behind the extra items when cycling
        if (isCycling) {
            gSPVertex(POLY_OPA_DISP++, sCycleCircleVtx, 8, 0);
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 0, 0, 0, pauseCtx->alpha * (1.0f - animScale));
            gDPLoadTextureBlock_4b(POLY_OPA_DISP++, gPausePromptCursorTex, G_IM_FMT_I, 48, 48, 0,
                                   G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                   G_TX_NOLOD, G_TX_NOLOD);

            if (showLeftItem) {
                gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
            }
            if (showRightItem) {
                gSP1Quadrangle(POLY_OPA_DISP++, 4, 6, 7, 5, 0);
            }
        }

        // Render left and right items
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
        gSPVertex(POLY_OPA_DISP++, sCycleExtraItemVtx, 8, 0);

        if (showLeftItem) {
            if (!CHECK_AGE_REQ_ITEM(leftItem)) {
                gDPSetGrayscaleColor(POLY_OPA_DISP++, 109, 109, 109, 255);
                gSPGrayscale(POLY_OPA_DISP++, true);
            }
            KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, gItemIcons[leftItem], 32, 32, 0);
            gSPGrayscale(POLY_OPA_DISP++, false);
        }
        if (showRightItem) {
            if (!CHECK_AGE_REQ_ITEM(rightItem)) {
                gDPSetGrayscaleColor(POLY_OPA_DISP++, 109, 109, 109, 255);
                gSPGrayscale(POLY_OPA_DISP++, true);
            }
            KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, gItemIcons[rightItem], 32, 32, 4);
            gSPGrayscale(POLY_OPA_DISP++, false);
        }

        Matrix_Pop();
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void KaleidoScope_HandleItemCycleExtras(PlayState* play, u8 slot, bool canCycle, u8 leftItem, u8 rightItem,
                                        bool replaceCButtons) {
    Input* input = &play->state.input[0];
    PauseContext* pauseCtx = &play->pauseCtx;
    bool dpad = (CVarGetInteger(CVAR_SETTING("DPadOnPause"), 0) && !CHECK_BTN_ALL(input->cur.button, BTN_CUP));
    u8 slotItem = gSaveContext.inventory.items[slot];
    u8 hasLeftItem = leftItem != ITEM_NONE && slotItem != leftItem;
    u8 hasRightItem = rightItem != ITEM_NONE && slotItem != rightItem && leftItem != rightItem;

    if (canCycle && pauseCtx->cursorSlot[PAUSE_ITEM] == slot && CHECK_BTN_ALL(input->press.button, BTN_A) &&
        (hasLeftItem || hasRightItem)) {
        Audio_PlaySfxGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        gCurrentItemCyclingSlot = gCurrentItemCyclingSlot == slot ? -1 : slot;
    }
    if (gCurrentItemCyclingSlot == slot) {
        pauseCtx->cursorColorSet = 8;
        if ((pauseCtx->stickRelX > 30 || pauseCtx->stickRelY > 30) ||
            dpad && CHECK_BTN_ANY(input->press.button, BTN_DRIGHT | BTN_DUP)) {
            Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            if (replaceCButtons) {
                for (int i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                    if (gSaveContext.equips.buttonItems[i] == gSaveContext.inventory.items[slot]) {
                        if (CHECK_AGE_REQ_ITEM(rightItem)) {
                            gSaveContext.equips.buttonItems[i] = rightItem;
                            Interface_LoadItemIcon1(play, i);
                        } else {
                            gSaveContext.equips.buttonItems[i] = ITEM_NONE;
                        }
                        break;
                    }
                }
            }
            gSaveContext.inventory.items[slot] = rightItem;
        } else if ((pauseCtx->stickRelX < -30 || pauseCtx->stickRelY < -30) ||
                   dpad && CHECK_BTN_ANY(input->press.button, BTN_DLEFT | BTN_DDOWN)) {
            Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            if (replaceCButtons) {
                for (int i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                    if (gSaveContext.equips.buttonItems[i] == gSaveContext.inventory.items[slot]) {
                        if (CHECK_AGE_REQ_ITEM(leftItem)) {
                            gSaveContext.equips.buttonItems[i] = leftItem;
                            Interface_LoadItemIcon1(play, i);
                        } else {
                            gSaveContext.equips.buttonItems[i] = ITEM_NONE;
                        }
                        break;
                    }
                }
            }
            gSaveContext.inventory.items[slot] = leftItem;
        }
        gCurrentItemCyclingSlot = pauseCtx->cursorSlot[PAUSE_ITEM] == slot ? slot : -1;
    }
}

bool CanMaskSelect() {
    if (IS_RANDO) {
        return true;
    }
    // only allow mask select when:
    // the shop is open:
    // * zelda's letter check: Flags_GetEventChkInf(EVENTCHKINF_OBTAINED_ZELDAS_LETTER)
    // * kak gate check: Flags_GetInfTable(INFTABLE_SHOWED_ZELDAS_LETTER_TO_GATE_GUARD)
    // and the mask quest is complete: Flags_GetEventChkInf(EVENTCHKINF_PAID_BACK_BUNNY_HOOD_FEE)
    return CVarGetInteger(CVAR_ENHANCEMENT("MaskSelect"), 0) &&
           Flags_GetEventChkInf(EVENTCHKINF_PAID_BACK_BUNNY_HOOD_FEE) &&
           Flags_GetEventChkInf(EVENTCHKINF_OBTAINED_ZELDAS_LETTER) &&
           Flags_GetInfTable(INFTABLE_SHOWED_ZELDAS_LETTER_TO_GATE_GUARD);
}

void KaleidoScope_HandleItemCycles(PlayState* play) {
    // handle the mask select
    KaleidoScope_HandleItemCycleExtras(
        play, SLOT_TRADE_CHILD, CanMaskSelect(),
        IS_RANDO ? Randomizer_GetPrevChildTradeItem()
                 : (INV_CONTENT(ITEM_TRADE_CHILD) <= ITEM_MASK_KEATON || INV_CONTENT(ITEM_TRADE_CHILD) > ITEM_MASK_TRUTH
                        ? ITEM_MASK_TRUTH
                        : INV_CONTENT(ITEM_TRADE_CHILD) - 1),
        IS_RANDO ? Randomizer_GetNextChildTradeItem()
                 : (INV_CONTENT(ITEM_TRADE_CHILD) >= ITEM_MASK_TRUTH || INV_CONTENT(ITEM_TRADE_CHILD) < ITEM_MASK_KEATON
                        ? ITEM_MASK_KEATON
                        : INV_CONTENT(ITEM_TRADE_CHILD) + 1),
        true);

    // handle the adult trade select
    KaleidoScope_HandleItemCycleExtras(play, SLOT_TRADE_ADULT,
                                       IS_RANDO && Randomizer_GetSettingValue(RSK_SHUFFLE_ADULT_TRADE),
                                       Randomizer_GetPrevAdultTradeItem(), Randomizer_GetNextAdultTradeItem(), true);

    // Handle Nayru's Love/Roc's Feather
    KaleidoScope_HandleItemCycleExtras(play, SLOT_NAYRUS_LOVE, Randomizer_GetSettingValue(RSK_ROCS_FEATHER),
                                       Enhancement_GetPrevNayrusItem(), Enhancement_GetNextNayrusItem(), true);
}

void KaleidoScope_DrawItemCycles(PlayState* play) {
    // draw the mask select
    KaleidoScope_DrawItemCycleExtras(
        play, SLOT_TRADE_CHILD, CanMaskSelect(),
        IS_RANDO ? Randomizer_GetPrevChildTradeItem()
                 : (INV_CONTENT(ITEM_TRADE_CHILD) <= ITEM_MASK_KEATON || INV_CONTENT(ITEM_TRADE_CHILD) > ITEM_MASK_TRUTH
                        ? ITEM_MASK_TRUTH
                        : INV_CONTENT(ITEM_TRADE_CHILD) - 1),
        IS_RANDO ? Randomizer_GetNextChildTradeItem()
                 : (INV_CONTENT(ITEM_TRADE_CHILD) >= ITEM_MASK_TRUTH || INV_CONTENT(ITEM_TRADE_CHILD) < ITEM_MASK_KEATON
                        ? ITEM_MASK_KEATON
                        : INV_CONTENT(ITEM_TRADE_CHILD) + 1));

    // draw the adult trade select
    KaleidoScope_DrawItemCycleExtras(play, SLOT_TRADE_ADULT,
                                     IS_RANDO && Randomizer_GetSettingValue(RSK_SHUFFLE_ADULT_TRADE),
                                     Randomizer_GetPrevAdultTradeItem(), Randomizer_GetNextAdultTradeItem());

    // Draw Nayru's Love/Roc's Feather
    KaleidoScope_DrawItemCycleExtras(play, SLOT_NAYRUS_LOVE, Randomizer_GetSettingValue(RSK_ROCS_FEATHER),
                                     Enhancement_GetPrevNayrusItem(), Enhancement_GetNextNayrusItem());
}

bool IsItemCycling() {
    return gCurrentItemCyclingSlot != -1;
}

void KaleidoScope_ResetItemCycling() {
    gCurrentItemCyclingSlot = -1;
}

#pragma endregion

// ---------------------------------------------------------------------------------------------
// Layout do menu de itens (estilo remake). Posicao do CENTRO de cada icone, em unidades de tela
// (320x240, X para a direita, Y para cima), relativas a grade original. Para mover um item, basta
// mudar o par {x, y} dele. A navegacao do cursor segue estas posicoes automaticamente.
// Colunas: -82, -46, -10, 26, 62, 98.  Linhas: 66, 32, -2, -36, -70.
// ---------------------------------------------------------------------------------------------
// ATENCAO: a ordem abaixo e a ordem dos SLOTs (0 a 23). Nao reordene as linhas; mude so os numeros.
s16 gKaleidoItemSlotLayout[24][2] = {
    { -82, 66 },  //  0 SLOT_DEKU_STICK
    { -82, -36 }, //  1 SLOT_DEKU_NUT
    { -82, -2 },  //  2 SLOT_BOMB
    { 26, 32 },   //  3 SLOT_BOW
    { -10, 66 },  //  4 SLOT_ARROW_FIRE
    { -10, -36 }, //  5 SLOT_DINS_FIRE
    { -46, 66 },  //  6 SLOT_SLINGSHOT
    { -46, -36 }, //  7 SLOT_OCARINA
    { -46, -2 },  //  8 SLOT_BOMBCHU
    { 62, 32 },   //  9 SLOT_HOOKSHOT
    { -10, 32 },  // 10 SLOT_ARROW_ICE
    { 26, -36 },  // 11 SLOT_FARORES_WIND
    { -46, 32 },  // 12 SLOT_BOOMERANG
    { 26, -2 },   // 13 SLOT_LENS_OF_TRUTH
    { -82, 32 },  // 14 SLOT_MAGIC_BEAN
    { 62, 66 },   // 15 SLOT_HAMMER
    { -10, -2 },  // 16 SLOT_ARROW_LIGHT
    { 62, -36 },  // 17 SLOT_NAYRUS_LOVE
    { -82, -70 }, // 18 SLOT_BOTTLE_1
    { -46, -70 }, // 19 SLOT_BOTTLE_2
    { -10, -70 }, // 20 SLOT_BOTTLE_3
    { 26, -70 },  // 21 SLOT_BOTTLE_4
    { 98, -36 },  // 22 SLOT_TRADE_ADULT
    { 62, -70 },  // 23 SLOT_TRADE_CHILD
};

// Acha o slot mais proximo de "from" na direcao (dx, dy). dx/dy valem -1, 0 ou 1 (dy > 0 = para cima).
// Retorna -1 se nao houver nenhum. Slots vazios so contam se anySlot for true (opcao "PauseAnyCursor").
static s16 KaleidoScope_FindItemSlotInDir(s16 from, s32 dx, s32 dy, bool anySlot) {
    s16 best = -1;
    s32 bestScore = 0x7FFFFFFF;
    s16 i;

    for (i = 0; i < 24; i++) {
        s32 offX;
        s32 offY;
        s32 along;
        s32 across;
        s32 score;

        if (i == from || (!anySlot && gSaveContext.inventory.items[i] == ITEM_NONE)) {
            continue;
        }

        offX = gKaleidoItemSlotLayout[i][0] - gKaleidoItemSlotLayout[from][0];
        offY = gKaleidoItemSlotLayout[i][1] - gKaleidoItemSlotLayout[from][1];
        along = dx * offX + dy * offY;
        across = ABS(dx != 0 ? offY : offX);

        // Precisa estar realmente "na frente" e dentro de um cone de ~63 graus.
        if (along <= 0 || across > along * 2) {
            continue;
        }

        score = along + across * 2;
        if (score < bestScore) {
            bestScore = score;
            best = i;
        }
    }

    return best;
}

// Slot ocupado mais a esquerda (fromLeft) ou mais a direita, preferindo o de cima. Usado ao entrar na
// grade vindo das setas de pagina.
static s16 KaleidoScope_FindItemSlotAtEdge(bool fromLeft, bool anySlot) {
    s16 best = -1;
    s16 i;

    for (i = 0; i < 24; i++) {
        s16 x = gKaleidoItemSlotLayout[i][0];
        s16 y = gKaleidoItemSlotLayout[i][1];

        if (!anySlot && gSaveContext.inventory.items[i] == ITEM_NONE) {
            continue;
        }

        if (best < 0) {
            best = i;
            continue;
        }

        s16 bx = gKaleidoItemSlotLayout[best][0];
        s16 by = gKaleidoItemSlotLayout[best][1];
        bool better = fromLeft ? (x < bx) : (x > bx);

        if (x == bx && y > by) {
            better = true;
        }
        if (better) {
            best = i;
        }
    }

    return best;
}

// ---------------------------------------------------------------------------------------------
// Estilo de cada slot (mesma ordem dos SLOTs, 0 a 23). Nao reordene; mude so os numeros.
//   icon = tamanho do icone, fw/fh = largura/altura da moldura,
//   cx/cy = quanto os cantos sao cortados (cx = fw/2 ou cy = fh/2 faz um hexagono).
// Use numeros PARES. Slots vizinhos ficam a ~34-36 unidades de distancia, entao molduras
// maiores que isso se sobrepoem.
// ---------------------------------------------------------------------------------------------
typedef struct {
    s16 icon;
    s16 fw;
    s16 fh;
    s16 cx;
    s16 cy;
} KaleidoSlotStyle;

#define STYLE_NORMAL { 28, 32, 32, 6, 6 }
// Hexagono com pontas em cima e embaixo. Para pontas nos lados, troque por { 28, 36, 34, 9, 17 }
#define STYLE_HEX { 28, 34, 36, 17, 9 }
#define STYLE_BIG { 34, 36, 36, 7, 7 }

static const KaleidoSlotStyle sSlotStyle[24] = {
    STYLE_NORMAL,             //  0 SLOT_DEKU_STICK
    STYLE_HEX,                //  1 SLOT_DEKU_NUT
    STYLE_NORMAL,             //  2 SLOT_BOMB
    { 40, 44, 44, 8, 8 },     //  3 SLOT_BOW (grande)
    STYLE_NORMAL,             //  4 SLOT_ARROW_FIRE
    STYLE_HEX,                //  5 SLOT_DINS_FIRE
    STYLE_NORMAL,             //  6 SLOT_SLINGSHOT
    STYLE_NORMAL,             //  7 SLOT_OCARINA
    STYLE_NORMAL,             //  8 SLOT_BOMBCHU
    STYLE_BIG,                //  9 SLOT_HOOKSHOT
    STYLE_NORMAL,             // 10 SLOT_ARROW_ICE
    STYLE_HEX,                // 11 SLOT_FARORES_WIND
    STYLE_NORMAL,             // 12 SLOT_BOOMERANG
    STYLE_NORMAL,             // 13 SLOT_LENS_OF_TRUTH
    STYLE_HEX,                // 14 SLOT_MAGIC_BEAN
    STYLE_BIG,                // 15 SLOT_HAMMER
    STYLE_NORMAL,             // 16 SLOT_ARROW_LIGHT
    STYLE_HEX,                // 17 SLOT_NAYRUS_LOVE
    STYLE_NORMAL,             // 18 SLOT_BOTTLE_1
    STYLE_NORMAL,             // 19 SLOT_BOTTLE_2
    STYLE_NORMAL,             // 20 SLOT_BOTTLE_3
    STYLE_NORMAL,             // 21 SLOT_BOTTLE_4
    STYLE_NORMAL,             // 22 SLOT_TRADE_ADULT
    STYLE_NORMAL,             // 23 SLOT_TRADE_CHILD
};

// 1 = desenha moldura tambem nos slots vazios (como o remake faz com os frascos), 0 = so onde ha item
#define KALEIDO_DRAW_EMPTY_FRAMES 0
// 1 = a moldura dourada substitui o cursor de cantos amarelos no menu em abas
#define KALEIDO_FRAME_AS_CURSOR 1

// Usado pelo z_kaleido_scope_PAL.c ao montar os vertices
s16 KaleidoScope_GetItemSlotIconSize(s16 slot) {
    return sSlotStyle[slot].icon;
}

// Cresce o quadrado do slot em 2 unidades por lado (efeito do slot selecionado)
static void KaleidoScope_GrowSlotVtx(Vtx* v) {
    s16 w = v[1].v.ob[0] - v[0].v.ob[0];
    s16 h = v[0].v.ob[1] - v[2].v.ob[1];

    v[0].v.ob[0] = v[2].v.ob[0] = v[0].v.ob[0] - 2;
    v[1].v.ob[0] = v[3].v.ob[0] = v[0].v.ob[0] + w + 4;
    v[0].v.ob[1] = v[1].v.ob[1] = v[0].v.ob[1] + 2;
    v[2].v.ob[1] = v[3].v.ob[1] = v[0].v.ob[1] - h - 4;
}

// Poligono de 8 pontos (retangulo com cantos cortados). Com cx = W ou cy = H vira hexagono.
static void KaleidoScope_SetPolyVtx(Vtx* v, s32 cx, s32 cy, s32 W, s32 H, s32 X, s32 Y) {
    s32 px[8] = { cx - W + X, cx + W - X, cx + W, cx + W, cx + W - X, cx - W + X, cx - W, cx - W };
    s32 py[8] = { cy + H, cy + H, cy + H - Y, cy - H + Y, cy - H, cy - H, cy - H + Y, cy + H - Y };
    s32 k;

    for (k = 0; k < 8; k++) {
        v[k].v.ob[0] = px[k];
        v[k].v.ob[1] = py[k];
        v[k].v.ob[2] = 0;
        v[k].v.flag = 0;
        v[k].v.tc[0] = v[k].v.tc[1] = 0;
        v[k].v.cn[0] = v[k].v.cn[1] = v[k].v.cn[2] = v[k].v.cn[3] = 255;
    }
}

static void KaleidoScope_DrawPoly(Gfx** dl, Vtx* v) {
    s32 k;

    gSPVertex((*dl)++, v, 8, 0);
    for (k = 1; k < 7; k++) {
        gSP1Triangle((*dl)++, 0, k, k + 1, 0);
    }
}

// Desenha a moldura de cada slot (borda + miolo) antes dos icones
static void KaleidoScope_DrawItemFrames(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    u8 alpha = (pauseCtx->alpha > 255) ? 255 : (u8)pauseCtx->alpha;
    bool cursorOn = (pauseCtx->state == 6) && (pauseCtx->cursorSpecialPos == 0) &&
                    (pauseCtx->unk_1E4 == 0 || pauseCtx->unk_1E4 == 8);
    s16 selected = pauseCtx->cursorSlot[PAUSE_ITEM];
    Vtx* vtx = (Vtx*)Graph_Alloc(gfxCtx, 24 * 16 * sizeof(Vtx));
    s16 i;

    OPEN_DISPS(gfxCtx);

    Gfx_SetupDL_39Opa(gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCycleType(POLY_OPA_DISP++, G_CYC_1CYCLE);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);

    for (i = 0; i < 24; i++) {
        const KaleidoSlotStyle* st = &sSlotStyle[i];
        bool hasItem = gSaveContext.inventory.items[i] != ITEM_NONE;
        bool isSel = cursorOn && (i == selected);
        s32 grow = (isSel && CHECK_AGE_REQ_SLOT(i)) ? 2 : 0;
        s32 b = isSel ? 2 : 1;
        s32 W = st->fw / 2 + grow;
        s32 H = st->fh / 2 + grow;
        s32 X = st->cx;
        s32 Y = st->cy;
        s32 iW = W - b;
        s32 iH = H - b;
        s32 iX = (X > iW) ? iW : X;
        s32 iY = (Y > iH) ? iH : Y;
        s32 cx = gKaleidoItemSlotLayout[i][0];
        s32 cy = gKaleidoItemSlotLayout[i][1] + pauseCtx->offsetY;
        Vtx* v = vtx + i * 16;

        if (!hasItem && !isSel && !KALEIDO_DRAW_EMPTY_FRAMES) {
            continue;
        }

        KaleidoScope_SetPolyVtx(v, cx, cy, W, H, X, Y);
        KaleidoScope_SetPolyVtx(v + 8, cx, cy, iW, iH, iX, iY);

        // Borda
        gDPPipeSync(POLY_OPA_DISP++);
        if (isSel) {
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 250, 230, alpha);
        } else {
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 96, 76, 50, alpha);
        }
        KaleidoScope_DrawPoly(&POLY_OPA_DISP, v);

        // Miolo
        gDPPipeSync(POLY_OPA_DISP++);
        if (isSel) {
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 176, 128, 24, alpha);
        } else {
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 22, 17, 13, alpha);
        }
        KaleidoScope_DrawPoly(&POLY_OPA_DISP, v + 8);
    }

    // Volta ao estado que o resto da pagina espera
    gDPPipeSync(POLY_OPA_DISP++);
    Gfx_SetupDL_42Opa(gfxCtx);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);

    CLOSE_DISPS(gfxCtx);
}

void KaleidoScope_DrawItemSelect(PlayState* play) {
    static s16 magicArrowEffectsR[] = { 255, 100, 255 };
    static s16 magicArrowEffectsG[] = { 0, 100, 255 };
    static s16 magicArrowEffectsB[] = { 0, 255, 100 };
    Input* input = &play->state.input[0];
    PauseContext* pauseCtx = &play->pauseCtx;
    u16 i;
    u16 j;
    u16 cursorItem;
    u16 cursorSlot = 0;
    u16 index;
    s16 cursorPoint;
    s16 cursorX;
    s16 cursorY;
    s16 oldCursorPoint;
    s16 moveCursorResult;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_42Opa(play->state.gfxCtx);

    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);

    pauseCtx->cursorColorSet = 0;
    pauseCtx->nameColorSet = 0;

    if ((pauseCtx->state == 6) && (pauseCtx->unk_1E4 == 0) && (pauseCtx->pageIndex == PAUSE_ITEM)) {
        bool dpad = (CVarGetInteger(CVAR_SETTING("DPadOnPause"), 0) && !CHECK_BTN_ALL(input->cur.button, BTN_CUP));
        bool pauseAnyCursor =
            pauseCtx->cursorSpecialPos == 0 &&
            ((CVarGetInteger(CVAR_ENHANCEMENT("PauseAnyCursor"), 0) == PAUSE_ANY_CURSOR_RANDO_ONLY && IS_RANDO) ||
             (CVarGetInteger(CVAR_ENHANCEMENT("PauseAnyCursor"), 0) == PAUSE_ANY_CURSOR_ALWAYS_ON));

        moveCursorResult = 0 || IsItemCycling();
        oldCursorPoint = pauseCtx->cursorPoint[PAUSE_ITEM];

        cursorItem = pauseCtx->cursorItem[PAUSE_ITEM];
        cursorSlot = pauseCtx->cursorSlot[PAUSE_ITEM];

        if (pauseCtx->cursorSpecialPos == 0) {
            pauseCtx->cursorColorSet = 4;

            if (cursorItem == PAUSE_ITEM_NONE) {
                pauseCtx->stickRelX = 40;
            }

            if ((ABS(pauseCtx->stickRelX) > 30) ||
                (dpad && CHECK_BTN_ANY(input->press.button, BTN_DLEFT | BTN_DRIGHT))) {
                cursorPoint = pauseCtx->cursorPoint[PAUSE_ITEM];
                cursorX = pauseCtx->cursorX[PAUSE_ITEM];
                cursorY = pauseCtx->cursorY[PAUSE_ITEM];

                osSyncPrintf("now=%d  ccc=%d\n", cursorPoint, cursorItem);

                // Seem necessary to match
                if (pauseCtx->cursorX[PAUSE_ITEM]) {}
                if (gSaveContext.inventory.items[pauseCtx->cursorPoint[PAUSE_ITEM]]) {}

                while (moveCursorResult == 0) {
                    bool goLeft =
                        (pauseCtx->stickRelX < -30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DLEFT));
                    s16 nextSlot = KaleidoScope_FindItemSlotInDir(pauseCtx->cursorPoint[PAUSE_ITEM],
                                                                  goLeft ? -1 : 1, 0, pauseAnyCursor);

                    if (nextSlot >= 0) {
                        pauseCtx->cursorPoint[PAUSE_ITEM] = nextSlot;
                        pauseCtx->cursorX[PAUSE_ITEM] = nextSlot % 6;
                        pauseCtx->cursorY[PAUSE_ITEM] = nextSlot / 6;
                        moveCursorResult = 1;
                    } else {
                        // Nada mais naquela direcao: passa para a aba/pagina vizinha.
                        pauseCtx->cursorX[PAUSE_ITEM] = cursorX;
                        pauseCtx->cursorY[PAUSE_ITEM] = cursorY;
                        pauseCtx->cursorPoint[PAUSE_ITEM] = cursorPoint;
                        KaleidoScope_MoveCursorToSpecialPos(play, goLeft ? PAUSE_CURSOR_PAGE_LEFT
                                                                         : PAUSE_CURSOR_PAGE_RIGHT);
                        moveCursorResult = 2;
                    }
                }

                if (moveCursorResult == 1) {
                    cursorItem = gSaveContext.inventory.items[pauseCtx->cursorPoint[PAUSE_ITEM]];
                }

                osSyncPrintf("【Ｘ cursor=%d(%) (cur_xpt=%d)(ok_fg=%d)(ccc=%d)(key_angle=%d)】  ",
                             pauseCtx->cursorPoint[PAUSE_ITEM], pauseCtx->cursorX[PAUSE_ITEM], moveCursorResult,
                             cursorItem, pauseCtx->cursorSpecialPos);
            }
        } else if (pauseCtx->cursorSpecialPos == PAUSE_CURSOR_PAGE_LEFT) {
            if ((pauseCtx->stickRelX > 30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DRIGHT))) {
                pauseCtx->nameDisplayTimer = 0;
                pauseCtx->cursorSpecialPos = 0;

                Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                     &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);

                cursorPoint = KaleidoScope_FindItemSlotAtEdge(true, false);
                if (cursorPoint >= 0) {
                    pauseCtx->cursorPoint[PAUSE_ITEM] = cursorPoint;
                    pauseCtx->cursorX[PAUSE_ITEM] = cursorPoint % 6;
                    pauseCtx->cursorY[PAUSE_ITEM] = cursorPoint / 6;
                    moveCursorResult = 1;
                } else {
                    KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_RIGHT);
                }
            }
        } else {
            if ((pauseCtx->stickRelX < -30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DLEFT))) {
                pauseCtx->nameDisplayTimer = 0;
                pauseCtx->cursorSpecialPos = 0;

                Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                     &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);

                cursorPoint = KaleidoScope_FindItemSlotAtEdge(false, false);
                if (cursorPoint >= 0) {
                    pauseCtx->cursorPoint[PAUSE_ITEM] = cursorPoint;
                    pauseCtx->cursorX[PAUSE_ITEM] = cursorPoint % 6;
                    pauseCtx->cursorY[PAUSE_ITEM] = cursorPoint / 6;
                    moveCursorResult = 1;
                } else {
                    KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_LEFT);
                }
            }
        }

        if (pauseCtx->cursorSpecialPos == 0) {
            if (cursorItem != PAUSE_ITEM_NONE) {
                if ((ABS(pauseCtx->stickRelY) > 30) ||
                    (dpad && CHECK_BTN_ANY(input->press.button, BTN_DDOWN | BTN_DUP))) {
                    moveCursorResult = 0 || IsItemCycling();

                    cursorPoint = pauseCtx->cursorPoint[PAUSE_ITEM];
                    cursorY = pauseCtx->cursorY[PAUSE_ITEM];
                    while (moveCursorResult == 0) {
                        bool goUp = (pauseCtx->stickRelY > 30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DUP));
                        s16 nextSlot = KaleidoScope_FindItemSlotInDir(pauseCtx->cursorPoint[PAUSE_ITEM], 0,
                                                                      goUp ? 1 : -1, pauseAnyCursor);

                        if (nextSlot >= 0) {
                            pauseCtx->cursorPoint[PAUSE_ITEM] = nextSlot;
                            pauseCtx->cursorX[PAUSE_ITEM] = nextSlot % 6;
                            pauseCtx->cursorY[PAUSE_ITEM] = nextSlot / 6;
                            moveCursorResult = 1;
                        } else {
                            pauseCtx->cursorY[PAUSE_ITEM] = cursorY;
                            pauseCtx->cursorPoint[PAUSE_ITEM] = cursorPoint;
                            moveCursorResult = 2;
                        }
                    }

                    cursorPoint = PAUSE_ITEM;
                    osSyncPrintf("【Ｙ cursor=%d(%) (cur_ypt=%d)(ok_fg=%d)(ccc=%d)】  ",
                                 pauseCtx->cursorPoint[cursorPoint], pauseCtx->cursorY[PAUSE_ITEM], moveCursorResult,
                                 cursorItem);
                }
            }

            cursorSlot = pauseCtx->cursorPoint[PAUSE_ITEM];

            pauseCtx->cursorColorSet = 4;

            if (moveCursorResult == 1) {
                cursorItem = gSaveContext.inventory.items[pauseCtx->cursorPoint[PAUSE_ITEM]];
            } else if (moveCursorResult != 2) {
                cursorItem = gSaveContext.inventory.items[pauseCtx->cursorPoint[PAUSE_ITEM]];
            }

            pauseCtx->cursorItem[PAUSE_ITEM] = cursorItem;
            pauseCtx->cursorSlot[PAUSE_ITEM] = cursorSlot;

            if (!CHECK_AGE_REQ_SLOT(cursorSlot)) {
                pauseCtx->nameColorSet = 1;
            }

            if (cursorItem != PAUSE_ITEM_NONE) {
                index = cursorSlot * 4; // required to match?
                KaleidoScope_SetCursorVtx(pauseCtx, index, pauseCtx->itemVtx);

                if ((pauseCtx->debugState == 0) && (pauseCtx->state == 6) && (pauseCtx->unk_1E4 == 0)) {
                    KaleidoScope_HandleItemCycles(play);
                    u16 buttonsToCheck = BTN_CLEFT | BTN_CDOWN | BTN_CRIGHT;
                    if (CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0) &&
                        (!CVarGetInteger(CVAR_SETTING("DPadOnPause"), 0) ||
                         CHECK_BTN_ALL(input->cur.button, BTN_CUP))) {
                        buttonsToCheck |= BTN_DUP | BTN_DDOWN | BTN_DLEFT | BTN_DRIGHT;
                    }
                    if (CHECK_BTN_ANY(input->press.button, buttonsToCheck)) {
                        if (CHECK_AGE_REQ_SLOT(cursorSlot) && (cursorItem != ITEM_SOLD_OUT) &&
                            (cursorItem != ITEM_NONE)) {
                            if (GameInteractor_Should(VB_EQUIP_ITEM_TO_C_BUTTON, true, play, cursorSlot, cursorItem)) {
                                KaleidoScope_SetupItemEquip(play, cursorItem, cursorSlot,
                                                            pauseCtx->itemVtx[index].v.ob[0] * 10,
                                                            pauseCtx->itemVtx[index].v.ob[1] * 10);
                            }
                        } else {
                            Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                        }
                    }
                }
            } else {
                pauseCtx->cursorVtx[0].v.ob[0] = pauseCtx->cursorVtx[2].v.ob[0] = pauseCtx->cursorVtx[1].v.ob[0] =
                    pauseCtx->cursorVtx[3].v.ob[0] = 0;

                pauseCtx->cursorVtx[0].v.ob[1] = pauseCtx->cursorVtx[1].v.ob[1] = pauseCtx->cursorVtx[2].v.ob[1] =
                    pauseCtx->cursorVtx[3].v.ob[1] = -200;
            }
        } else {
            pauseCtx->cursorItem[PAUSE_ITEM] = PAUSE_ITEM_NONE;
        }

        if (oldCursorPoint != pauseCtx->cursorPoint[PAUSE_ITEM]) {
            Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
    } else if ((pauseCtx->unk_1E4 == 3) && (pauseCtx->pageIndex == PAUSE_ITEM)) {
        KaleidoScope_SetCursorVtx(pauseCtx, cursorSlot * 4, pauseCtx->itemVtx);
        pauseCtx->cursorColorSet = 4;
    }

    if (CVarGetInteger(CVAR_ENHANCEMENT("TabbedPauseMenu"), 1) != 0) {
        KaleidoScope_DrawItemFrames(play);
    }

    gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 0);

    for (i = 0, j = 24 * 4; i < ARRAY_COUNT(gSaveContext.equips.cButtonSlots); i++, j += 4) {
        if ((gSaveContext.equips.buttonItems[i + 1] != ITEM_NONE) &&
            !((gSaveContext.equips.buttonItems[i + 1] >= ITEM_SHIELD_DEKU) &&
              (gSaveContext.equips.buttonItems[i + 1] <= ITEM_BOOTS_HOVER))) {
            gSPVertex(POLY_OPA_DISP++, &pauseCtx->itemVtx[j], 4, 0);
            POLY_OPA_DISP = KaleidoScope_QuadTextureIA8(POLY_OPA_DISP, gEquippedItemOutlineTex, 32, 32, 0);
        }
    }

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);

    for (i = j = 0; i < 24; i++, j += 4) {
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);

        if (gSaveContext.inventory.items[i] != ITEM_NONE) {
            if ((pauseCtx->unk_1E4 == 0) && (pauseCtx->pageIndex == PAUSE_ITEM) && (pauseCtx->cursorSpecialPos == 0)) {
                if (CHECK_AGE_REQ_SLOT(i)) {
                    if ((sEquipState == 2) && (i == 3)) {
                        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, magicArrowEffectsR[pauseCtx->equipTargetItem - 0xBF],
                                        magicArrowEffectsG[pauseCtx->equipTargetItem - 0xBF],
                                        magicArrowEffectsB[pauseCtx->equipTargetItem - 0xBF], pauseCtx->alpha);

                        KaleidoScope_GrowSlotVtx(&pauseCtx->itemVtx[j]);
                    } else if (i == cursorSlot) {
                        KaleidoScope_GrowSlotVtx(&pauseCtx->itemVtx[j]);
                    }
                }
            }

            gSPVertex(POLY_OPA_DISP++, &pauseCtx->itemVtx[j + 0], 4, 0);
            int itemId = gSaveContext.inventory.items[i];
            bool not_acquired = !CHECK_AGE_REQ_ITEM(itemId);
            if (not_acquired) {
                gDPSetGrayscaleColor(POLY_OPA_DISP++, 109, 109, 109, 255);
                gSPGrayscale(POLY_OPA_DISP++, true);
            }
            KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, gItemIcons[itemId], 32, 32, 0);
            gSPGrayscale(POLY_OPA_DISP++, false);
        }
    }

    if (pauseCtx->cursorSpecialPos == 0 &&
        !(KALEIDO_FRAME_AS_CURSOR && CVarGetInteger(CVAR_ENHANCEMENT("TabbedPauseMenu"), 1) != 0)) {
        KaleidoScope_DrawCursor(play, PAUSE_ITEM);
    }

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);

    u8 gBetterAmmoRendering = CVarGetInteger(CVAR_ENHANCEMENT("BetterAmmoRendering"), 0);

    for (i = 0; i < (gBetterAmmoRendering ? 24 : 15); i++) {
        if ((gBetterAmmoRendering ? ItemInSlotUsesAmmo(i) : gAmmoItems[i] != ITEM_NONE) &&
            (gSaveContext.inventory.items[i] != ITEM_NONE)) {
            KaleidoScope_DrawAmmoCount(pauseCtx, play->state.gfxCtx, gSaveContext.inventory.items[i], i);
        }
    }

    KaleidoScope_DrawItemCycles(play);

    CLOSE_DISPS(play->state.gfxCtx);
}

void KaleidoScope_SetupItemEquip(PlayState* play, u16 item, u16 slot, s16 animX, s16 animY) {
    Input* input = &play->state.input[0];
    PauseContext* pauseCtx = &play->pauseCtx;
    KaleidoScope_ResetItemCycling();

    if (CHECK_BTN_ALL(input->press.button, BTN_CLEFT)) {
        pauseCtx->equipTargetCBtn = 0;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_CDOWN)) {
        pauseCtx->equipTargetCBtn = 1;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_CRIGHT)) {
        pauseCtx->equipTargetCBtn = 2;
    } else if (CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0)) {
        if (CHECK_BTN_ALL(input->press.button, BTN_DUP)) {
            pauseCtx->equipTargetCBtn = 3;
        } else if (CHECK_BTN_ALL(input->press.button, BTN_DDOWN)) {
            pauseCtx->equipTargetCBtn = 4;
        } else if (CHECK_BTN_ALL(input->press.button, BTN_DLEFT)) {
            pauseCtx->equipTargetCBtn = 5;
        } else if (CHECK_BTN_ALL(input->press.button, BTN_DRIGHT)) {
            pauseCtx->equipTargetCBtn = 6;
        }
    }

    pauseCtx->equipTargetItem = item;
    pauseCtx->equipTargetSlot = slot;
    pauseCtx->unk_1E4 = 3;
    pauseCtx->equipAnimX = animX;
    pauseCtx->equipAnimY = animY;
    pauseCtx->equipAnimAlpha = 255;
    sEquipAnimTimer = 0;
    sEquipState = 3;
    sEquipMoveTimer = 10;
    if ((pauseCtx->equipTargetItem == ITEM_ARROW_FIRE) || (pauseCtx->equipTargetItem == ITEM_ARROW_ICE) ||
        (pauseCtx->equipTargetItem == ITEM_ARROW_LIGHT)) {
        if (CVarGetInteger(CVAR_ENHANCEMENT("SkipArrowAnimation"), 0)) {
            Audio_PlaySfxGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        } else {
            u16 index = 0;
            if (pauseCtx->equipTargetItem == ITEM_ARROW_ICE) {
                index = 1;
            }
            if (pauseCtx->equipTargetItem == ITEM_ARROW_LIGHT) {
                index = 2;
            }
            Audio_PlaySfxGeneral(NA_SE_SY_SET_FIRE_ARROW + index, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            pauseCtx->equipTargetItem = 0xBF + index;
            sEquipState = 0;
            pauseCtx->equipAnimAlpha = 0;
            sEquipMoveTimer = 6;
        }
    } else {
        Audio_PlaySfxGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
}

// TODO update for final positions
static s16 sCButtonPosX[] = { 66, 90, 114, 110, 110, 86, 134 };
static s16 sCButtonPosY[] = { 110, 92, 110, 76, 44, 62, 62 };

void KaleidoScope_UpdateItemEquip(PlayState* play) {
    static s16 D_8082A488 = 0;
    PauseContext* pauseCtx = &play->pauseCtx;
    Vtx* bowItemVtx;
    u16 offsetX;
    u16 offsetY;

    s16 Top_HUD_Margin = CVarGetInteger(CVAR_COSMETIC("HUD.Margin.T"), 0);
    s16 Left_HUD_Margin = CVarGetInteger(CVAR_COSMETIC("HUD.Margin.L"), 0);
    s16 Right_HUD_Margin = CVarGetInteger(CVAR_COSMETIC("HUD.Margin.R"), 0);
    s16 Bottom_HUD_Margin = CVarGetInteger(CVAR_COSMETIC("HUD.Margin.B"), 0);

    s16 X_Margins_CL;
    s16 X_Margins_CR;
    s16 X_Margins_CD;
    s16 Y_Margins_CL;
    s16 Y_Margins_CR;
    s16 Y_Margins_CD;
    s16 X_Margins_BtnB;
    s16 Y_Margins_BtnB;
    s16 X_Margins_DPad_Items;
    s16 Y_Margins_DPad_Items;
    if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.BButton.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_BtnB = Right_HUD_Margin;
        };
        Y_Margins_BtnB = (Top_HUD_Margin * -1);
    } else {
        X_Margins_BtnB = 0;
        Y_Margins_BtnB = 0;
    }
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_CL = Right_HUD_Margin;
        };
        Y_Margins_CL = (Top_HUD_Margin * -1);
    } else {
        X_Margins_CL = 0;
        Y_Margins_CL = 0;
    }
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_CR = Right_HUD_Margin;
        };
        Y_Margins_CR = (Top_HUD_Margin * -1);
    } else {
        X_Margins_CR = 0;
        Y_Margins_CR = 0;
    }
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_CD = Right_HUD_Margin;
        };
        Y_Margins_CD = (Top_HUD_Margin * -1);
    } else {
        X_Margins_CD = 0;
        Y_Margins_CD = 0;
    }
    if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.UseMargins"), 0) != 0) {
        if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) == ORIGINAL_LOCATION) {
            X_Margins_DPad_Items = Right_HUD_Margin;
        };
        Y_Margins_DPad_Items = (Top_HUD_Margin * -1);
    } else {
        X_Margins_DPad_Items = 0;
        Y_Margins_DPad_Items = 0;
    }
    const s16 ItemIconPos_ori[7][2] = { { C_LEFT_BUTTON_X + X_Margins_CL, C_LEFT_BUTTON_Y + Y_Margins_CL },
                                        { C_DOWN_BUTTON_X + X_Margins_CD, C_DOWN_BUTTON_Y + Y_Margins_CD },
                                        { C_RIGHT_BUTTON_X + X_Margins_CR, C_RIGHT_BUTTON_Y + Y_Margins_CR },
                                        { DPAD_UP_X + X_Margins_DPad_Items, DPAD_UP_Y + Y_Margins_DPad_Items },
                                        { DPAD_DOWN_X + X_Margins_DPad_Items, DPAD_DOWN_Y + Y_Margins_DPad_Items },
                                        { DPAD_LEFT_X + X_Margins_DPad_Items, DPAD_LEFT_Y + Y_Margins_DPad_Items },
                                        { DPAD_RIGHT_X + X_Margins_DPad_Items, DPAD_RIGHT_Y + Y_Margins_DPad_Items } };
    s16 DPad_ItemsOffset[4][2] = {
        { 7, -8 }, // Up
        { 7, 24 }, // Down
        { -9, 8 }, // Left
        { 23, 8 }, // Right
    };             //(X,Y) Used with custom position to place it properly.

    // DPadItems
    if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) != ORIGINAL_LOCATION) {
        sCButtonPosY[3] =
            CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosY"), 0) + Y_Margins_DPad_Items + DPad_ItemsOffset[0][1]; // Up
        sCButtonPosY[4] =
            CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosY"), 0) + Y_Margins_DPad_Items + DPad_ItemsOffset[1][1]; // Down
        sCButtonPosY[5] =
            CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosY"), 0) + Y_Margins_DPad_Items + DPad_ItemsOffset[2][1]; // Left
        sCButtonPosY[6] =
            CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosY"), 0) + Y_Margins_DPad_Items + DPad_ItemsOffset[3][1]; // Right
        if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) == ANCHOR_LEFT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.UseMargins"), 0) != 0) {
                X_Margins_DPad_Items = Left_HUD_Margin;
            };
            sCButtonPosX[3] = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                          X_Margins_DPad_Items + DPad_ItemsOffset[0][0]);
            sCButtonPosX[4] = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                          X_Margins_DPad_Items + DPad_ItemsOffset[1][0]);
            sCButtonPosX[5] = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                          X_Margins_DPad_Items + DPad_ItemsOffset[2][0]);
            sCButtonPosX[6] = OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                          X_Margins_DPad_Items + DPad_ItemsOffset[3][0]);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) == ANCHOR_RIGHT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.UseMargins"), 0) != 0) {
                X_Margins_DPad_Items = Right_HUD_Margin;
            };
            sCButtonPosX[3] = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                           X_Margins_DPad_Items + DPad_ItemsOffset[0][0]);
            sCButtonPosX[4] = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                           X_Margins_DPad_Items + DPad_ItemsOffset[1][0]);
            sCButtonPosX[5] = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                           X_Margins_DPad_Items + DPad_ItemsOffset[2][0]);
            sCButtonPosX[6] = OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) +
                                                           X_Margins_DPad_Items + DPad_ItemsOffset[3][0]);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosType"), 0) == ANCHOR_NONE) {
            sCButtonPosX[3] = CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) + DPad_ItemsOffset[0][0];
            sCButtonPosX[4] = CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) + DPad_ItemsOffset[1][0];
            sCButtonPosX[5] = CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) + DPad_ItemsOffset[2][0];
            sCButtonPosX[6] = CVarGetInteger(CVAR_COSMETIC("HUD.Dpad.PosX"), 0) + DPad_ItemsOffset[3][0];
        }
    } else {
        sCButtonPosX[3] = OTRGetDimensionFromRightEdge(ItemIconPos_ori[3][0]);
        sCButtonPosX[4] = OTRGetDimensionFromRightEdge(ItemIconPos_ori[4][0]);
        sCButtonPosX[5] = OTRGetDimensionFromRightEdge(ItemIconPos_ori[5][0]);
        sCButtonPosX[6] = OTRGetDimensionFromRightEdge(ItemIconPos_ori[6][0]);
        sCButtonPosY[3] = ItemIconPos_ori[3][1];
        sCButtonPosY[4] = ItemIconPos_ori[4][1];
        sCButtonPosY[5] = ItemIconPos_ori[5][1];
        sCButtonPosY[6] = ItemIconPos_ori[6][1];
    }
    // C button Left
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) != ORIGINAL_LOCATION) {
        sCButtonPosY[0] = CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosY"), 0) + Y_Margins_CL;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == ANCHOR_LEFT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.UseMargins"), 0) != 0) {
                X_Margins_CL = Left_HUD_Margin;
            };
            sCButtonPosX[0] =
                OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosX"), 0) + X_Margins_CL);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == ANCHOR_RIGHT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.UseMargins"), 0) != 0) {
                X_Margins_CL = Right_HUD_Margin;
            };
            sCButtonPosX[0] =
                OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosX"), 0) + X_Margins_CL);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosType"), 0) == ANCHOR_NONE) {
            sCButtonPosX[0] = CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.PosX"), 0);
        }
    } else {
        sCButtonPosX[0] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[0][0]);
        sCButtonPosY[0] = ItemIconPos_ori[0][1];
    }
    // C Button down
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) != ORIGINAL_LOCATION) {
        sCButtonPosY[1] = CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosY"), 0) + Y_Margins_CD;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ANCHOR_LEFT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.UseMargins"), 0) != 0) {
                X_Margins_CD = Left_HUD_Margin;
            };
            sCButtonPosX[1] =
                OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosX"), 0) + X_Margins_CD);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ANCHOR_RIGHT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.UseMargins"), 0) != 0) {
                X_Margins_CD = Right_HUD_Margin;
            };
            sCButtonPosX[1] =
                OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosX"), 0) + X_Margins_CD);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosType"), 0) == ANCHOR_NONE) {
            sCButtonPosX[1] = CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.PosX"), 0);
        }
    } else {
        sCButtonPosX[1] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[1][0]);
        sCButtonPosY[1] = ItemIconPos_ori[1][1];
    }
    // C button Right
    if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) != ORIGINAL_LOCATION) {
        sCButtonPosY[2] = CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosY"), 0) + Y_Margins_CR;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == ANCHOR_LEFT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.UseMargins"), 0) != 0) {
                X_Margins_CR = Left_HUD_Margin;
            };
            sCButtonPosX[2] =
                OTRGetDimensionFromLeftEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosX"), 0) + X_Margins_CR);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == ANCHOR_RIGHT) {
            if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.UseMargins"), 0) != 0) {
                X_Margins_CR = Right_HUD_Margin;
            };
            sCButtonPosX[2] =
                OTRGetDimensionFromRightEdge(CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosX"), 0) + X_Margins_CR);
        } else if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosType"), 0) == ANCHOR_NONE) {
            sCButtonPosX[2] = CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.PosX"), 0);
        }
    } else {
        sCButtonPosX[2] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[2][0]);
        sCButtonPosY[2] = ItemIconPos_ori[2][1];
    }

    sCButtonPosX[0] = sCButtonPosX[0] - 160;
    sCButtonPosY[0] = 120 - sCButtonPosY[0];
    sCButtonPosX[1] = sCButtonPosX[1] - 160;
    sCButtonPosY[1] = 120 - sCButtonPosY[1];
    sCButtonPosX[2] = sCButtonPosX[2] - 160;
    sCButtonPosY[2] = 120 - sCButtonPosY[2];
    sCButtonPosX[3] = sCButtonPosX[3] - 160;
    sCButtonPosY[3] = 120 - sCButtonPosY[3];
    sCButtonPosX[4] = sCButtonPosX[4] - 160;
    sCButtonPosY[4] = 120 - sCButtonPosY[4];
    sCButtonPosX[5] = sCButtonPosX[5] - 160;
    sCButtonPosY[5] = 120 - sCButtonPosY[5];
    sCButtonPosX[6] = sCButtonPosX[6] - 160;
    sCButtonPosY[6] = 120 - sCButtonPosY[6];

    if (sEquipState == 0) {
        pauseCtx->equipAnimAlpha += 14;
        if (pauseCtx->equipAnimAlpha > 255) {
            pauseCtx->equipAnimAlpha = 254;
            sEquipState++;
        }
        sEquipAnimTimer = 5;
        return;
    }

    if (sEquipState == 2) {
        D_8082A488--;

        if (D_8082A488 == 0) {
            pauseCtx->equipTargetItem -= 0xBF - ITEM_BOW_ARROW_FIRE;
            if (!CVarGetInteger(CVAR_ENHANCEMENT("SeparateArrows"), 0)) {
                pauseCtx->equipTargetSlot = SLOT_BOW;
            }
            sEquipMoveTimer = 6;
            WREG(90) = 320;
            WREG(87) = WREG(91);
            sEquipState++;
            Audio_PlaySfxGeneral(NA_SE_SY_SYNTH_MAGIC_ARROW, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
        return;
    }

    if (sEquipState == 1) {
        bowItemVtx = &pauseCtx->itemVtx[12];
        offsetX = ABS(pauseCtx->equipAnimX - bowItemVtx->v.ob[0] * 10) / sEquipMoveTimer;
        offsetY = ABS(pauseCtx->equipAnimY - bowItemVtx->v.ob[1] * 10) / sEquipMoveTimer;
    } else {
        offsetX = ABS(pauseCtx->equipAnimX - sCButtonPosX[pauseCtx->equipTargetCBtn] * 10) / sEquipMoveTimer;
        offsetY = ABS(pauseCtx->equipAnimY - sCButtonPosY[pauseCtx->equipTargetCBtn] * 10) / sEquipMoveTimer;
    }

    if ((pauseCtx->equipTargetItem >= 0xBF) && (pauseCtx->equipAnimAlpha < 254)) {
        pauseCtx->equipAnimAlpha += 14;
        if (pauseCtx->equipAnimAlpha > 255) {
            pauseCtx->equipAnimAlpha = 254;
        }
        sEquipAnimTimer = 5;
        return;
    }

    if (sEquipAnimTimer == 0) {
        WREG(90) -= WREG(87) / sEquipMoveTimer;
        WREG(87) -= WREG(87) / sEquipMoveTimer;

        if (sEquipState == 1) {
            if (pauseCtx->equipAnimX >= (pauseCtx->itemVtx[12].v.ob[0] * 10)) {
                pauseCtx->equipAnimX -= offsetX;
            } else {
                pauseCtx->equipAnimX += offsetX;
            }

            if (pauseCtx->equipAnimY >= (pauseCtx->itemVtx[12].v.ob[1] * 10)) {
                pauseCtx->equipAnimY -= offsetY;
            } else {
                pauseCtx->equipAnimY += offsetY;
            }
        } else {
            if (pauseCtx->equipAnimX >= sCButtonPosX[pauseCtx->equipTargetCBtn] * 10) {
                pauseCtx->equipAnimX -= offsetX;
            } else {
                pauseCtx->equipAnimX += offsetX;
            }

            if (pauseCtx->equipAnimY >= sCButtonPosY[pauseCtx->equipTargetCBtn] * 10) {
                pauseCtx->equipAnimY -= offsetY;
            } else {
                pauseCtx->equipAnimY += offsetY;
            }
        }

        sEquipMoveTimer--;

        if (sEquipMoveTimer == 0) {
            if (sEquipState == 1) {
                sEquipState++;
                D_8082A488 = 4;
                return;
            }

            osSyncPrintf("\n＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝\n");

            // Skipping the arrow animation: need to change the item's type and
            // slot when it hits the button since it didn't get set earlier
            if (pauseCtx->equipTargetItem == ITEM_ARROW_FIRE || pauseCtx->equipTargetItem == ITEM_ARROW_ICE ||
                pauseCtx->equipTargetItem == ITEM_ARROW_LIGHT) {
                switch (pauseCtx->equipTargetItem) {
                    case ITEM_ARROW_FIRE:
                        pauseCtx->equipTargetItem = ITEM_BOW_ARROW_FIRE;
                        break;
                    case ITEM_ARROW_ICE:
                        pauseCtx->equipTargetItem = ITEM_BOW_ARROW_ICE;
                        break;
                    case ITEM_ARROW_LIGHT:
                        pauseCtx->equipTargetItem = ITEM_BOW_ARROW_LIGHT;
                        break;
                }
                if (!CVarGetInteger(CVAR_ENHANCEMENT("SeparateArrows"), 0)) {
                    pauseCtx->equipTargetSlot = SLOT_BOW;
                }
            }

            // If the item is on another button already, swap the two
            uint16_t targetButtonIndex = pauseCtx->equipTargetCBtn + 1;
            for (uint16_t otherSlotIndex = 0; otherSlotIndex < ARRAY_COUNT(gSaveContext.equips.cButtonSlots);
                 otherSlotIndex++) {
                uint16_t otherButtonIndex = otherSlotIndex + 1;
                if (otherSlotIndex == pauseCtx->equipTargetCBtn) {
                    continue;
                }

                if (pauseCtx->equipTargetSlot == gSaveContext.equips.cButtonSlots[otherSlotIndex]) {
                    // Assign the other button to the target's current item
                    if (gSaveContext.equips.buttonItems[targetButtonIndex] != ITEM_NONE) {
                        gSaveContext.equips.buttonItems[otherButtonIndex] =
                            gSaveContext.equips.buttonItems[targetButtonIndex];
                        gSaveContext.equips.cButtonSlots[otherSlotIndex] =
                            gSaveContext.equips.cButtonSlots[pauseCtx->equipTargetCBtn];
                        Interface_LoadItemIcon2(play, otherButtonIndex);
                    } else {
                        gSaveContext.equips.buttonItems[otherButtonIndex] = ITEM_NONE;
                        gSaveContext.equips.cButtonSlots[otherSlotIndex] = SLOT_NONE;
                    }
                    // break; // 'Assume there is only one possible pre-existing equip'
                }

                // Fix for Equip Dupe
                if (pauseCtx->equipTargetItem == ITEM_BOW) {
                    if (gSaveContext.equips.buttonItems[otherButtonIndex] >= ITEM_BOW_ARROW_FIRE &&
                        gSaveContext.equips.buttonItems[otherButtonIndex] <= ITEM_BOW_ARROW_LIGHT &&
                        !CVarGetInteger(CVAR_ENHANCEMENT("SeparateArrows"), 0)) {
                        gSaveContext.equips.buttonItems[otherButtonIndex] =
                            gSaveContext.equips.buttonItems[targetButtonIndex];
                        gSaveContext.equips.cButtonSlots[otherSlotIndex] =
                            gSaveContext.equips.cButtonSlots[pauseCtx->equipTargetCBtn];
                        Interface_LoadItemIcon2(play, otherButtonIndex);
                    }
                }
            }

            gSaveContext.equips.buttonItems[targetButtonIndex] = pauseCtx->equipTargetItem;
            gSaveContext.equips.cButtonSlots[pauseCtx->equipTargetCBtn] = pauseCtx->equipTargetSlot;
            Interface_LoadItemIcon1(play, targetButtonIndex);

            pauseCtx->unk_1E4 = 0;
            sEquipMoveTimer = 10;
            WREG(90) = 320;
            WREG(87) = WREG(91);
        }
    } else {
        sEquipAnimTimer--;
        if (sEquipAnimTimer == 0) {
            pauseCtx->equipAnimAlpha = 255;
        }
    }
}
