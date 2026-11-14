// ---- "Better Save Menu": janelinha de salvar na parte de baixo da tela ----
// Incluir em z_kaleido_scope_PAL.c ENTRE o fim de KaleidoScope_DrawMapSidebar e KaleidoScope_Draw:
//     #include "z_kaleido_save_popup.inc"
// Usa helpers que ja existem no arquivo: KaleidoScope_SetupFillState, KaleidoScope_TabBarFill,
// KaleidoScope_TabBarText, KaleidoScope_MapSidebarSfx e KaleidoScope_UseSavePopup (definida perto de
// MAP_PAGE_SHIFT_X, ver PATCH.md).
//
// Estados reaproveitando pauseCtx->unk_1EC (so enquanto pauseCtx->state == 7 e a janelinha esta ativa):
//   0/1 = perguntando (Sim/Nao), 4 = "Jogo salvo!" visivel. Nao ha rotacao do cubo nem pagina de save.

#include "z_kaleido_save_popup_tex.h"

#define SAVE_POPUP_L 48
#define SAVE_POPUP_T 144
#define SAVE_POPUP_R 272
#define SAVE_POPUP_B 208
#define SAVE_POPUP_ROW_Y0 (SAVE_POPUP_T + 24)
#define SAVE_POPUP_ROW_H 12
#define SAVE_POPUP_ROW_STEP 15

static s16 sSavePopupTimer = 0;

// Volta para o menu (state 6) na mesma aba, sem fechar o menu de pausa.
static void KaleidoScope_SavePopupClose(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;

    Interface_SetDoAction(play, DO_ACTION_NONE);
    gSaveContext.buttonStatus[0] = gSaveContext.buttonStatus[1] = gSaveContext.buttonStatus[2] =
        gSaveContext.buttonStatus[3] = BTN_ENABLED;
    gSaveContext.buttonStatus[5] = gSaveContext.buttonStatus[6] = gSaveContext.buttonStatus[7] =
        gSaveContext.buttonStatus[8] = BTN_ENABLED;
    gSaveContext.hudVisibilityMode = 0;
    Interface_ChangeHudVisibilityMode(50);
    func_800F64E0(0);

    pauseCtx->promptChoice = 0;
    pauseCtx->unk_1F4 = pauseCtx->unk_1F8 = pauseCtx->unk_1FC = pauseCtx->unk_200 = 0.0f;
    pauseCtx->unk_204 = -314.0f;
    pauseCtx->unk_1EC = 0;
    pauseCtx->state = 6;
}

// Chamada de dentro do `case 7:` de KaleidoScope_Update. Devolve true quando a janelinha cuidou do frame.
static bool KaleidoScope_SavePopupUpdate(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = &play->state.input[0];

    if (!KaleidoScope_UseSavePopup(pauseCtx)) {
        return false;
    }

    pauseCtx->unk_204 = -314.0f; // sem rotacao: o cubo fica parado na pagina atual

    if (pauseCtx->unk_1EC < 4) {
        pauseCtx->unk_1EC = 1;

        // Sim (0) em cima, Nao (1) embaixo
        if (CHECK_BTN_ALL(input->press.button, BTN_DUP) || input->rel.stick_y > 30) {
            if (pauseCtx->promptChoice != 0) {
                pauseCtx->promptChoice = 0;
                KaleidoScope_MapSidebarSfx(NA_SE_SY_CURSOR);
            }
        } else if (CHECK_BTN_ALL(input->press.button, BTN_DDOWN) || input->rel.stick_y < -30) {
            if (pauseCtx->promptChoice != 1) {
                pauseCtx->promptChoice = 1;
                KaleidoScope_MapSidebarSfx(NA_SE_SY_CURSOR);
            }
        }

        if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
            if (pauseCtx->promptChoice == 0) {
                KaleidoScope_MapSidebarSfx(NA_SE_SY_PIECE_OF_HEART);
                Play_PerformSave(play);
                if (CVarGetInteger(CVAR_ENHANCEMENT("SkipSaveConfirmation"), 0)) {
                    KaleidoScope_SavePopupClose(play);
                } else {
                    pauseCtx->unk_1EC = 4;
                    sSavePopupTimer = 60; // 2 s (a 30 fps de logica)
                }
            } else {
                KaleidoScope_MapSidebarSfx(NA_SE_SY_DECIDE);
                KaleidoScope_SavePopupClose(play);
            }
        } else if (CHECK_BTN_ALL(input->press.button, BTN_B) || CHECK_BTN_ALL(input->press.button, BTN_START)) {
            KaleidoScope_SavePopupClose(play);
        }
    } else {
        if (CHECK_BTN_ALL(input->press.button, BTN_A) || CHECK_BTN_ALL(input->press.button, BTN_B) ||
            CHECK_BTN_ALL(input->press.button, BTN_START) || (--sSavePopupTimer <= 0)) {
            KaleidoScope_SavePopupClose(play);
        }
    }

    return true;
}

static void KaleidoScope_DrawSavePopup(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    u8 alpha = (pauseCtx->alpha > 255) ? 255 : (u8)pauseCtx->alpha;
    bool saved = pauseCtx->unk_1EC >= 4;
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);

    KaleidoScope_SetupFillState(play);

    // Escurece a tela inteira (inclusive as bordas em widescreen) para dar foco a janelinha
    KaleidoScope_TabBarFill(play, OTRGetRectDimensionFromLeftEdge(0), 0, OTRGetRectDimensionFromRightEdge(SCREEN_WIDTH),
                            SCREEN_HEIGHT, 0, 0, 0, (u8)(alpha * 120 / 255));

    // Janela: borda dourada de 1 unidade + fundo escuro
    KaleidoScope_TabBarFill(play, SAVE_POPUP_L, SAVE_POPUP_T, SAVE_POPUP_R, SAVE_POPUP_B, 120, 92, 52, alpha);
    KaleidoScope_TabBarFill(play, SAVE_POPUP_L + 1, SAVE_POPUP_T + 1, SAVE_POPUP_R - 1, SAVE_POPUP_B - 1, 16, 12, 10,
                            alpha);

    if (!saved) {
        for (i = 0; i < 2; i++) {
            s32 x0 = SAVE_POPUP_L + 10;
            s32 x1 = SAVE_POPUP_R - 10;
            s32 y0 = SAVE_POPUP_ROW_Y0 + i * SAVE_POPUP_ROW_STEP;
            s32 y1 = y0 + SAVE_POPUP_ROW_H;

            if (i == pauseCtx->promptChoice) {
                KaleidoScope_TabBarFill(play, x0, y0, x1, y1, 92, 68, 40, alpha);
                KaleidoScope_TabBarFill(play, x0, y0, x0 + 3, y1, 236, 200, 116, alpha); // barra dourada
            } else {
                KaleidoScope_TabBarFill(play, x0, y0, x1, y1, 34, 26, 20, alpha);
            }
        }
    }
    gDPPipeSync(POLY_OPA_DISP++);

    // Textos (IA8 coloridos pela cor primaria, igual a sidebar)
    gDPSetCombineLERP(POLY_OPA_DISP++, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0,
                      TEXEL0, 0, PRIMITIVE, 0);
    gDPSetTextureFilter(POLY_OPA_DISP++, G_TF_BILERP);

    if (saved) {
        KaleidoScope_TabBarText(play, sSavePopupTexSaved, SAVE_POPUP_TEX_SAVED_W,
                                SAVE_POPUP_L + ((SAVE_POPUP_R - SAVE_POPUP_L) - SAVE_POPUP_TEX_SAVED_W / 4) / 2,
                                SAVE_POPUP_T + ((SAVE_POPUP_B - SAVE_POPUP_T) - 10) / 2, 236, 200, 116, alpha);
    } else {
        KaleidoScope_TabBarText(play, sSavePopupTexAsk, SAVE_POPUP_TEX_ASK_W, SAVE_POPUP_L + 14, SAVE_POPUP_T + 8, 255,
                                244, 214, alpha);

        for (i = 0; i < 2; i++) {
            s32 y = SAVE_POPUP_ROW_Y0 + i * SAVE_POPUP_ROW_STEP + (SAVE_POPUP_ROW_H - 10) / 2;
            u8* tex = (i == 0) ? sSavePopupTexYes : sSavePopupTexNo;
            s32 w = (i == 0) ? SAVE_POPUP_TEX_YES_W : SAVE_POPUP_TEX_NO_W;

            if (i == pauseCtx->promptChoice) {
                KaleidoScope_TabBarText(play, tex, w, SAVE_POPUP_L + 22, y, 255, 244, 214, alpha);
            } else {
                KaleidoScope_TabBarText(play, tex, w, SAVE_POPUP_L + 22, y, 190, 176, 146, alpha);
            }
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
