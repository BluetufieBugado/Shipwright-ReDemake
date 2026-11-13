#include "z_kaleido_scope.h"
#include "textures/parameter_static/parameter_static.h"
#include "textures/icon_item_static/icon_item_static.h"
#include "soh/Enhancements/cosmetics/cosmeticsTypes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include <libultraship/bridge/consolevariablebridge.h>

extern const char* digitTextures[];

// ---------------------------------------------------------------------------------------------------------------
// Aba Colecao no menu em abas (como no remake): medalhoes nos vertices de um hexagono, pedras espirituais na faixa
// central, e uma coluna a direita com Skulltula Dourada, Peca de Coracao, Pedra da Agonia e Cartao Gerudo.
// As musicas sairam desta aba. O fundo, as molduras e o painel de detalhes sao desenhados em z_kaleido_scope_PAL.c
// (KaleidoScope_DrawQuestFrames / KaleidoScope_DrawQuestDetailPanel); aqui ficam o layout, a navegacao e os icones.
// ---------------------------------------------------------------------------------------------------------------
#define QUEST_SLOT_COUNT 25
#define QUEST_SLOT_HEART_PIECE 24

typedef struct {
    s16 x;    // centro do slot, em unidades da PAGINA (as mesmas dos vertices; x p/ direita, y p/ cima)
    s16 y;    // idem
    s16 icon; // lado do icone, em unidades da pagina (0 = slot que nao existe nesta aba)
    s16 ring; // raio da moldura redonda, em unidades de tela
} QuestSlotLayout;

// Tela = 160 + x * 1,03 ; 120 - y * 1,03. O layout fica em unidades da pagina para o icone e a moldura cairem exatamente
// no mesmo ponto (antes eu convertia de tela para pagina truncando e o icone saia ate 1 unidade do lugar).

// Indice = numero do slot do jogo (QUEST_MEDALLION_FOREST = 0 ... heart piece = 24). Slots 6..17 eram as musicas.
static const QuestSlotLayout sQuestSlotLayout[QUEST_SLOT_COUNT] = {
    { 99, 28, 28, 19 },   // 0  Medalhao da Floresta (vertice superior direito)
    { 99, -32, 28, 19 },  // 1  Medalhao do Fogo (inferior direito)
    { 47, -62, 28, 19 },  // 2  Medalhao da Agua (inferior)
    { -5, -32, 28, 19 },  // 3  Medalhao do Espirito (inferior esquerdo)
    { -5, 28, 28, 19 },   // 4  Medalhao da Sombra (superior esquerdo)
    { 47, 58, 28, 19 },   // 5  Medalhao da Luz (superior)
    { 0, 0, 0, 0 },       // 6..17 musicas: nao existem mais aqui
    { 0, 0, 0, 0 },       { 0, 0, 0, 0 },       { 0, 0, 0, 0 },       { 0, 0, 0, 0 },       { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },       { 0, 0, 0, 0 },       { 0, 0, 0, 0 },       { 0, 0, 0, 0 },       { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 15, -2, 24, 14 },   // 18 Esmeralda Kokiri (centro do hexagono)
    { 47, -2, 24, 14 },   // 19 Rubi Goron (e o centro do hexagono)
    { 79, -2, 24, 14 },   // 20 Safira Zora
    { 136, -19, 24, 15 }, // 21 Pedra da Agonia (coluna da direita)
    { 136, -55, 24, 15 }, // 22 Cartao Gerudo
    { 136, 68, 24, 15 },  // 23 Skulltula Dourada (o contador fica logo abaixo)
    { 136, 18, 24, 15 },  // 24 Peca de Coracao
};

// Navegacao: { cima, baixo, esquerda, direita }. -1 = nada, -3 = seta de pagina da esquerda, -2 = da direita.
static const s8 sQuestNav[QUEST_SLOT_COUNT][4] = {
    { 5, 1, 20, 24 },      // 0  Floresta
    { 0, 2, 20, 21 },      // 1  Fogo
    { 19, -1, 3, 1 },      // 2  Agua
    { 4, 2, -3, 18 },      // 3  Espirito
    { 5, 3, -3, 18 },      // 4  Sombra
    { -1, 19, 4, 0 },      // 5  Luz
    { -1, -1, -1, -1 },    // 6..17
    { -1, -1, -1, -1 },    { -1, -1, -1, -1 },    { -1, -1, -1, -1 },    { -1, -1, -1, -1 },
    { -1, -1, -1, -1 },    { -1, -1, -1, -1 },    { -1, -1, -1, -1 },    { -1, -1, -1, -1 },
    { -1, -1, -1, -1 },    { -1, -1, -1, -1 },    { -1, -1, -1, -1 },
    { 5, 2, 4, 19 },       // 18 Kokiri
    { 5, 2, 18, 20 },      // 19 Goron
    { 0, 1, 19, 24 },      // 20 Zora
    { 24, 22, 1, -2 },     // 21 Agonia
    { 21, -1, 1, -2 },     // 22 Cartao Gerudo
    { -1, 24, 0, -2 },     // 23 Skulltula
    { 23, 21, 0, -2 },     // 24 Peca de Coracao
};

bool KaleidoScope_IsTabbedQuestPage(PauseContext* pauseCtx) {
    return CVarGetInteger(CVAR_ENHANCEMENT("TabbedPauseMenu"), 1) != 0 && pauseCtx->pageIndex == PAUSE_QUEST &&
           !pauseCtx->randoQuestMode;
}

// Centro do slot em QUARTOS de unidade de tela (precisao de ~1 pixel) e raio da moldura em unidades de tela
bool KaleidoScope_GetQuestSlotCenterQ(s16 slot, s32* xq, s32* yq, s16* ring) {
    if (slot < 0 || slot >= QUEST_SLOT_COUNT || sQuestSlotLayout[slot].icon == 0) {
        return false;
    }
    *xq = (s32)(4.0f * (160.0f + sQuestSlotLayout[slot].x * 1.03f) + 0.5f);
    *yq = (s32)(4.0f * (120.0f - sQuestSlotLayout[slot].y * 1.03f) + 0.5f);
    *ring = sQuestSlotLayout[slot].ring;
    return true;
}

bool KaleidoScope_IsQuestSlotOwned(s16 slot) {
    if (slot < 0 || slot >= QUEST_SLOT_COUNT || sQuestSlotLayout[slot].icon == 0) {
        return false;
    }
    if (slot == QUEST_SLOT_HEART_PIECE) {
        return (gSaveContext.inventory.questItems & 0xF0000000) != 0;
    }
    return CHECK_QUEST_ITEM(slot);
}

// Posiciona os quadrilateros (questVtx) de cada slot e dos digitos da Skulltula. Chamado pelo InitVertices a cada
// quadro, depois do preenchimento padrao. So muda posicao/tamanho (ob); as coordenadas de textura continuam as do jogo.
void KaleidoScope_SetQuestTabbedVertices(PauseContext* pauseCtx) {
    s16 slot;
    s16 k;
    s16 n;
    s16 pcx;
    s16 ytop;

    for (slot = 0; slot < QUEST_SLOT_COUNT; slot++) {
        const QuestSlotLayout* l = &sQuestSlotLayout[slot];
        Vtx* v = &pauseCtx->questVtx[slot * 4];
        s16 half = l->icon / 2;
        s16 px = l->x;
        s16 py = l->y;

        if (l->icon == 0) {
            continue;
        }

        v[0].v.ob[0] = v[2].v.ob[0] = px - half;
        v[1].v.ob[0] = v[3].v.ob[0] = px + half;
        v[0].v.ob[1] = v[1].v.ob[1] = py + half + pauseCtx->offsetY;
        v[2].v.ob[1] = v[3].v.ob[1] = py - half + pauseCtx->offsetY;
    }

    // Contador de Skulltulas: ate 3 digitos, centralizado logo abaixo do icone. Quadrilateros 41..43 = sombra,
    // 44..46 = digitos (o jogo so desenha os digitos que existem; o ultimo sempre).
    n = (gSaveContext.inventory.gsTokens >= 100) ? 3 : (gSaveContext.inventory.gsTokens >= 10) ? 2 : 1;
    pcx = sQuestSlotLayout[23].x;
    ytop = sQuestSlotLayout[23].y - 18;

    for (k = 0; k < 3; k++) {
        s16 x0 = pcx - 12 + k * 8 + (4 * n - 12);
        Vtx* shadow = &pauseCtx->questVtx[(41 + k) * 4];
        Vtx* digit = &pauseCtx->questVtx[(44 + k) * 4];

        digit[0].v.ob[0] = digit[2].v.ob[0] = x0;
        digit[1].v.ob[0] = digit[3].v.ob[0] = x0 + 8;
        digit[0].v.ob[1] = digit[1].v.ob[1] = ytop + pauseCtx->offsetY;
        digit[2].v.ob[1] = digit[3].v.ob[1] = ytop + pauseCtx->offsetY - 16;

        shadow[0].v.ob[0] = shadow[2].v.ob[0] = x0 + 2;
        shadow[1].v.ob[0] = shadow[3].v.ob[0] = x0 + 10;
        shadow[0].v.ob[1] = shadow[1].v.ob[1] = ytop + pauseCtx->offsetY - 2;
        shadow[2].v.ob[1] = shadow[3].v.ob[1] = ytop + pauseCtx->offsetY - 18;
    }
}

static void KaleidoScope_QuestCursorSfx(void) {
    Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// Id do icone que o jogo guarda em cursorItem para cada slot (vazio = PAUSE_ITEM_NONE)
static u16 KaleidoScope_QuestSlotCursorItem(s16 slot) {
    if (slot == QUEST_SLOT_HEART_PIECE) {
        return ((gSaveContext.inventory.questItems & 0xF0000000) != 0) ? 0x72 : PAUSE_ITEM_NONE;
    }
    if (!KaleidoScope_IsQuestSlotOwned(slot)) {
        return PAUSE_ITEM_NONE;
    }
    return (slot < 6) ? (slot + 0x66) : (slot + 0x5A);
}

void KaleidoScope_DrawQuestStatusTabbed(PlayState* play, GraphicsContext* gfxCtx) {
    static s16 sHeartPulse[][4] = {
        { 255, 0, 0, 255 },
        { 255, 70, 0, 150 },
        { 255, 70, 0, 150 },
        { 255, 0, 0, 255 },
    };
    static s16 sHeartR = 0;
    static s16 sHeartG = 0;
    static s16 sHeartB = 0;
    static s16 sHeartA = 0;
    static s16 sHeartTimer = 20;
    static s16 sHeartIdx = 0;
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = &play->state.input[0];
    s16 slot;
    s16 i;
    s16 dr;
    s16 dg;
    s16 db;
    s16 da;
    s16 digits[3];
    s16 shown;
    s16 row;

    OPEN_DISPS(gfxCtx);

    // Cursor num slot que nao existe mais nesta aba (ex.: parado numa antiga musica): volta para a Luz
    if (pauseCtx->cursorPoint[PAUSE_QUEST] < 0 || pauseCtx->cursorPoint[PAUSE_QUEST] >= QUEST_SLOT_COUNT ||
        sQuestSlotLayout[pauseCtx->cursorPoint[PAUSE_QUEST]].icon == 0) {
        pauseCtx->cursorPoint[PAUSE_QUEST] = 5;
        pauseCtx->cursorSlot[PAUSE_QUEST] = 5;
    }

    if (pauseCtx->pageIndex == PAUSE_QUEST) {
        if (CVarGetInteger(CVAR_SETTING("DPadOnPause"), 0)) {
            if (CHECK_BTN_ALL(input->press.button, BTN_DLEFT)) {
                pauseCtx->stickRelX = -35;
            } else if (CHECK_BTN_ALL(input->press.button, BTN_DRIGHT)) {
                pauseCtx->stickRelX = 35;
            } else if (CHECK_BTN_ALL(input->press.button, BTN_DDOWN)) {
                pauseCtx->stickRelY = -35;
            } else if (CHECK_BTN_ALL(input->press.button, BTN_DUP)) {
                pauseCtx->stickRelY = 35;
            }
        }
        pauseCtx->cursorColorSet = 0;

        if (pauseCtx->cursorSpecialPos == 0) {
            pauseCtx->nameColorSet = 0;

            if ((pauseCtx->state != 6) || ((pauseCtx->stickRelX == 0) && (pauseCtx->stickRelY == 0))) {
                slot = pauseCtx->cursorSlot[PAUSE_QUEST];
            } else {
                s16 prev = pauseCtx->cursorPoint[PAUSE_QUEST];
                s16 next = prev;
                s16 target;
                bool special = false;

                if (pauseCtx->stickRelX < -30 || pauseCtx->stickRelX > 30) {
                    target = sQuestNav[prev][(pauseCtx->stickRelX < 0) ? 2 : 3];
                    if (target == -3) {
                        KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_LEFT);
                        special = true;
                    } else if (target == -2) {
                        KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_RIGHT);
                        special = true;
                    } else if (target >= 0) {
                        next = target;
                    }
                }

                if (!special && next == prev && (pauseCtx->stickRelY < -30 || pauseCtx->stickRelY > 30)) {
                    target = sQuestNav[prev][(pauseCtx->stickRelY < 0) ? 1 : 0];
                    if (target >= 0) {
                        next = target;
                    }
                }

                pauseCtx->cursorPoint[PAUSE_QUEST] = next;
                if (next != prev) {
                    KaleidoScope_QuestCursorSfx();
                }

                slot = pauseCtx->cursorPoint[PAUSE_QUEST];
                pauseCtx->cursorItem[pauseCtx->pageIndex] = KaleidoScope_QuestSlotCursorItem(slot);
                pauseCtx->cursorSlot[pauseCtx->pageIndex] = slot;
            }

            KaleidoScope_SetCursorVtx(pauseCtx, slot * 4, pauseCtx->questVtx);
        } else {
            // Cursor nas setas de pagina: volta para a grade ao apertar na direcao oposta
            s16 back = (pauseCtx->cursorSpecialPos == PAUSE_CURSOR_PAGE_LEFT) ? 4 : QUEST_SLOT_HEART_PIECE;
            bool leaves = (pauseCtx->cursorSpecialPos == PAUSE_CURSOR_PAGE_LEFT) ? (pauseCtx->stickRelX > 30)
                                                                                 : (pauseCtx->stickRelX < -30);

            if (leaves) {
                pauseCtx->cursorPoint[PAUSE_QUEST] = back;
                pauseCtx->nameDisplayTimer = 0;
                pauseCtx->cursorSpecialPos = 0;
                slot = back;
                KaleidoScope_SetCursorVtx(pauseCtx, slot * 4, pauseCtx->questVtx);
                KaleidoScope_QuestCursorSfx();
                pauseCtx->cursorItem[pauseCtx->pageIndex] = KaleidoScope_QuestSlotCursorItem(slot);
                pauseCtx->cursorSlot[pauseCtx->pageIndex] = slot;
            } else {
                slot = pauseCtx->cursorSlot[PAUSE_QUEST];
            }
        }
    } else {
        slot = pauseCtx->cursorSlot[PAUSE_QUEST];
        KaleidoScope_SetCursorVtx(pauseCtx, slot * 4, pauseCtx->questVtx);
    }

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);

    // Medalhoes
    for (i = 0; i < 6; i++) {
        if (CHECK_QUEST_ITEM(i)) {
            gDPPipeSync(POLY_OPA_DISP++);
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
            gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 0);
            gSPVertex(POLY_OPA_DISP++, &pauseCtx->questVtx[i * 4], 4, 0);
            KaleidoScope_DrawQuadTextureRGBA32(gfxCtx, gItemIcons[ITEM_MEDALLION_FOREST + i], 24, 24, 0);
        }
    }

    // Pedras espirituais
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);

    for (i = 0; i < 3; i++) {
        if (CHECK_QUEST_ITEM(18 + i)) {
            gSPVertex(POLY_OPA_DISP++, &pauseCtx->questVtx[(18 + i) * 4], 4, 0);
            KaleidoScope_DrawQuadTextureRGBA32(gfxCtx, gItemIcons[ITEM_KOKIRI_EMERALD + i], 24, 24, 0);
        }
    }

    // Pedra da Agonia, Cartao Gerudo e Skulltula Dourada
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);

    for (i = 0; i < 3; i++) {
        if (CHECK_QUEST_ITEM(21 + i)) {
            gSPVertex(POLY_OPA_DISP++, &pauseCtx->questVtx[(21 + i) * 4], 4, 0);
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
            KaleidoScope_DrawQuadTextureRGBA32(gfxCtx, gItemIcons[ITEM_STONE_OF_AGONY + i], 24, 24, 0);
        }
    }

    // Pulso de cor da Peca de Coracao (mesma animacao do jogo original)
    if (sHeartTimer <= 0) {
        sHeartTimer = 1;
    }
    dr = ABS(sHeartR - sHeartPulse[sHeartIdx][0]) / sHeartTimer;
    dg = ABS(sHeartG - sHeartPulse[sHeartIdx][1]) / sHeartTimer;
    db = ABS(sHeartB - sHeartPulse[sHeartIdx][2]) / sHeartTimer;
    da = ABS(sHeartA - sHeartPulse[sHeartIdx][3]) / sHeartTimer;
    sHeartR += (sHeartR >= sHeartPulse[sHeartIdx][0]) ? -dr : dr;
    sHeartG += (sHeartG >= sHeartPulse[sHeartIdx][1]) ? -dg : dg;
    sHeartB += (sHeartB >= sHeartPulse[sHeartIdx][2]) ? -db : db;
    sHeartA += (sHeartA >= sHeartPulse[sHeartIdx][3]) ? -da : da;

    if (--sHeartTimer == 0) {
        sHeartR = sHeartPulse[sHeartIdx][0];
        sHeartG = sHeartPulse[sHeartIdx][1];
        sHeartB = sHeartPulse[sHeartIdx][2];
        sHeartA = sHeartPulse[sHeartIdx][3];
        sHeartTimer = ZREG(24 + sHeartIdx);
        if (++sHeartIdx >= 4) {
            sHeartIdx = 0;
        }
    }

    if ((gSaveContext.inventory.questItems >> 0x1C) != 0) {
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                          PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);

        if ((pauseCtx->state == 4) || (pauseCtx->state == 0x12)) {
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, sHeartPulse[0][0], sHeartPulse[0][1], sHeartPulse[0][2],
                            pauseCtx->alpha);
        } else {
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, sHeartR, sHeartG, sHeartB, sHeartA);
        }

        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
        gSPVertex(POLY_OPA_DISP++, &pauseCtx->questVtx[QUEST_SLOT_HEART_PIECE * 4], 4, 0);

        POLY_OPA_DISP = KaleidoScope_QuadTextureIA8(
            POLY_OPA_DISP,
            gItemIcons[0x79 + (((gSaveContext.inventory.questItems & 0xF0000000) & 0xF0000000) >> 0x1C)], 48, 48, 0);
    }

    // Contador de Skulltulas Douradas (digitos do jogo; vermelho ao chegar em 100)
    if (CHECK_QUEST_ITEM(QUEST_SKULL_TOKEN)) {
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                          PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 0);

        digits[0] = digits[1] = 0;
        digits[2] = gSaveContext.inventory.gsTokens;

        while (digits[2] >= 100) {
            digits[0]++;
            digits[2] -= 100;
        }

        while (digits[2] >= 10) {
            digits[1]++;
            digits[2] -= 10;
        }

        gSPVertex(POLY_OPA_DISP++, &pauseCtx->questVtx[164], 24, 0);

        for (row = 0, i = 0; row < 2; row++) {
            if (row == 0) {
                gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 0, 0, 0, pauseCtx->alpha);
            } else if (gSaveContext.inventory.gsTokens == 100) {
                gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 200, 50, 50, pauseCtx->alpha);
            } else {
                gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
            }

            shown = 0;
            for (slot = 0; slot < 3; slot++, i += 4) {
                if ((slot >= 2) || (digits[slot] != 0) || (shown != 0)) {
                    gDPLoadTextureBlock(POLY_OPA_DISP++, digitTextures[digits[slot]], G_IM_FMT_I, G_IM_SIZ_8b, 8, 16,
                                        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                                        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

                    gSP1Quadrangle(POLY_OPA_DISP++, i, i + 2, i + 3, i + 1, 0);

                    shown = 1;
                }
            }
        }
    }

    CLOSE_DISPS(gfxCtx);
}

void KaleidoScope_DrawQuestStatus(PlayState* play, GraphicsContext* gfxCtx) {

    static s16 D_8082A070[][4] = {
        { 255, 0, 0, 255 },
        { 255, 70, 0, 150 },
        { 255, 70, 0, 150 },
        { 255, 0, 0, 255 },
    };
    static s16 D_8082A090[][3] = {
        { 0, 0, 0 },  { 0, 0, 0 },  { 0, 0, 0 },    { 0, 0, 0 },   { 0, 0, 0 },   { 0, 0, 0 },
        { 0, 60, 0 }, { 90, 0, 0 }, { 0, 40, 110 }, { 80, 40, 0 }, { 70, 0, 90 }, { 90, 90, 0 },
    };
    static s16 D_8082A0D8[] = { 255, 255, 255, 255, 255, 255 };
    static s16 D_8082A0E4[] = { 255, 255, 255, 255, 255, 255 };
    static s16 D_8082A0F0[] = { 150, 150, 150, 150, 150, 150 };
    static s16 D_8082A0FC = 20;
    static s16 D_8082A100 = 0;
    static s16 D_8082A104 = 0;
    static s16 D_8082A108 = 0;
    static s16 D_8082A10C = 0;
    static s16 D_8082A110 = 0;
    static s16 D_8082A114 = 20;
    static s16 D_8082A118 = 0;
    static s16 D_8082A11C = 0;
    static s16 D_8082A120 = 0;
    static u8 D_8082A124[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    static void* D_8082A130[] = {
        gOcarinaBtnIconATex,     gOcarinaBtnIconCDownTex, gOcarinaBtnIconCRightTex,
        gOcarinaBtnIconCLeftTex, gOcarinaBtnIconCUpTex,
    };
    static u16 D_8082A144[] = {
        0xFFCC, 0xFFCC, 0xFFCC, 0xFFCC, 0xFFCC,
    };
    static s16 D_8082A150[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    static s16 D_8082A164[] = {
        150, 255, 100, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    };
    static s16 D_8082A17C[] = {
        255, 80, 150, 160, 100, 240, 255, 255, 255, 255, 255, 255,
    };
    static s16 D_8082A194[] = {
        100, 40, 255, 0, 255, 100, 255, 255, 255, 255, 255, 255,
    };
    static s8 D_8082A1AC[][4] = {
        { 0x05, 0x01, 0x05, 0xFE }, { 0x00, 0x02, 0x02, 0xFE }, { 0xFF, 0x13, 0x03, 0x01 }, { 0x04, 0x02, 0x11, 0x02 },
        { 0x05, 0x03, 0x18, 0x05 }, { 0xFF, 0xFF, 0x04, 0x00 }, { 0x0C, 0xFF, 0xFD, 0x07 }, { 0x0D, 0xFF, 0x06, 0x08 },
        { 0x0E, 0xFF, 0x07, 0x09 }, { 0x0F, 0xFF, 0x08, 0x0A }, { 0x10, 0xFF, 0x09, 0x0B }, { 0x11, 0xFF, 0x0A, 0x12 },
        { 0x17, 0x06, 0xFD, 0x0D }, { 0x17, 0x07, 0x0C, 0x0E }, { 0x17, 0x08, 0x0D, 0x0F }, { 0x18, 0x09, 0x0E, 0x10 },
        { 0x18, 0x0A, 0x0F, 0x11 }, { 0x18, 0x0B, 0x10, 0x03 }, { 0x02, 0xFF, 0x0B, 0x13 }, { 0x02, 0xFF, 0x12, 0x14 },
        { 0x02, 0xFF, 0x13, 0xFE }, { 0xFF, 0x17, 0xFD, 0x16 }, { 0xFF, 0x17, 0x15, 0x18 }, { 0x15, 0x0C, 0xFD, 0x18 },
        { 0xFF, 0x10, 0x16, 0x04 }, { 0x00, 0x00, 0x00, 0x00 },
    };
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = &play->state.input[0];
    s16 sp226;
    s16 sp224;
    s16 sp222;
    s16 sp220;
    s16 phi_s0;
    s16 phi_s3;
    s16 sp21A;
    s16 sp218;
    s16 sp216 = -1;
    s16 pad1;
    s16 phi_v1;
    s16 pad2;
    s16 phi_s0_2;
    s16 sp208[3];

    // Menu em abas: a aba Colecao usa o layout novo (sem as musicas)
    if (KaleidoScope_IsTabbedQuestPage(pauseCtx)) {
        KaleidoScope_DrawQuestStatusTabbed(play, gfxCtx);
        return;
    }

    OPEN_DISPS(gfxCtx);

    if (((pauseCtx->unk_1E4 == 0) || (pauseCtx->unk_1E4 == 5) || (pauseCtx->unk_1E4 == 8)) &&
        (pauseCtx->pageIndex == PAUSE_QUEST)) {
        if (CVarGetInteger(CVAR_SETTING("DPadOnPause"), 0)) {
            if (CHECK_BTN_ALL(input->press.button, BTN_DLEFT)) {
                pauseCtx->stickRelX = -35;
            } else if (CHECK_BTN_ALL(input->press.button, BTN_DRIGHT)) {
                pauseCtx->stickRelX = 35;
            } else if (CHECK_BTN_ALL(input->press.button, BTN_DDOWN)) {
                pauseCtx->stickRelY = -35;
            } else if (CHECK_BTN_ALL(input->press.button, BTN_DUP)) {
                pauseCtx->stickRelY = 35;
            }
        }
        pauseCtx->cursorColorSet = 0;

        if (pauseCtx->cursorSpecialPos == 0) {
            pauseCtx->nameColorSet = 0;
            if ((pauseCtx->state != 6) || ((pauseCtx->stickRelX == 0) && (pauseCtx->stickRelY == 0))) {
                // No cursor movement
                sp216 = pauseCtx->cursorSlot[PAUSE_QUEST];
            } else {
                phi_s3 = pauseCtx->cursorPoint[PAUSE_QUEST];

                if ((pauseCtx->stickRelX < -30)) {
                    phi_s0 = D_8082A1AC[phi_s3][2];
                    if (phi_s0 == -3) {
                        KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_LEFT);
                        pauseCtx->unk_1E4 = 0;
                    } else {
                        while (phi_s0 >= 0) {
                            if ((s16)KaleidoScope_UpdateQuestStatusPoint(pauseCtx, phi_s0) != 0) {
                                break;
                            }
                            phi_s0 = D_8082A1AC[phi_s0][2];
                        }
                    }
                } else if ((pauseCtx->stickRelX > 30)) {
                    phi_s0 = D_8082A1AC[phi_s3][3];
                    if (phi_s0 == -2) {
                        KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_RIGHT);
                        pauseCtx->unk_1E4 = 0;
                    } else {
                        while (phi_s0 >= 0) {
                            if ((s16)KaleidoScope_UpdateQuestStatusPoint(pauseCtx, phi_s0) != 0) {
                                break;
                            }
                            phi_s0 = D_8082A1AC[phi_s0][3];
                        }
                    }
                }

                if ((pauseCtx->stickRelY < -30)) {
                    phi_s0 = D_8082A1AC[phi_s3][1];
                    while (phi_s0 >= 0) {
                        if ((s16)KaleidoScope_UpdateQuestStatusPoint(pauseCtx, phi_s0) != 0) {
                            break;
                        }
                        phi_s0 = D_8082A1AC[phi_s0][1];
                    }
                } else if ((pauseCtx->stickRelY > 30)) {
                    phi_s0 = D_8082A1AC[phi_s3][0];
                    while (phi_s0 >= 0) {
                        if ((s16)KaleidoScope_UpdateQuestStatusPoint(pauseCtx, phi_s0) != 0) {
                            break;
                        }
                        phi_s0 = D_8082A1AC[phi_s0][0];
                    }
                }

                if (phi_s3 != pauseCtx->cursorPoint[PAUSE_QUEST]) {
                    pauseCtx->unk_1E4 = 0;
                    Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                }

                if (pauseCtx->cursorPoint[PAUSE_QUEST] != 0x18) {
                    if (CHECK_QUEST_ITEM(pauseCtx->cursorPoint[PAUSE_QUEST])) {
                        if (pauseCtx->cursorPoint[PAUSE_QUEST] < 6) {
                            phi_s0_2 = pauseCtx->cursorPoint[PAUSE_QUEST] + 0x66;
                            osSyncPrintf("000 ccc=%d\n", phi_s0_2);
                        } else if (pauseCtx->cursorPoint[PAUSE_QUEST] < 0x12) {
                            phi_s0_2 = pauseCtx->cursorPoint[PAUSE_QUEST] + 0x54;
                            osSyncPrintf("111 ccc=%d\n", phi_s0_2);
                        } else {
                            phi_s0_2 = pauseCtx->cursorPoint[PAUSE_QUEST] + 0x5A;
                            osSyncPrintf("222 ccc=%d (%d, %d, %d)\n", phi_s0_2, pauseCtx->cursorPoint[PAUSE_QUEST],
                                         0x12, 0x6C);
                        }
                    } else {
                        phi_s0_2 = PAUSE_ITEM_NONE;
                        osSyncPrintf("999 ccc=%d (%d,  %d)\n", PAUSE_ITEM_NONE, pauseCtx->cursorPoint[PAUSE_QUEST],
                                     0x18);
                    }
                } else {
                    if ((gSaveContext.inventory.questItems & 0xF0000000) != 0) {
                        phi_s0_2 = 0x72;
                    } else {
                        phi_s0_2 = PAUSE_ITEM_NONE;
                    }
                    osSyncPrintf("888 ccc=%d (%d,  %d,  %x)\n", phi_s0_2, pauseCtx->cursorPoint[PAUSE_QUEST], 0x72,
                                 gSaveContext.inventory.questItems & 0xF0000000);
                }

                sp216 = pauseCtx->cursorPoint[PAUSE_QUEST];
                pauseCtx->cursorItem[pauseCtx->pageIndex] = phi_s0_2;
                pauseCtx->cursorSlot[pauseCtx->pageIndex] = sp216;
            }

            KaleidoScope_SetCursorVtx(pauseCtx, sp216 * 4, pauseCtx->questVtx);

            if ((pauseCtx->state == 6) && (pauseCtx->unk_1E4 == 0) && (pauseCtx->cursorSpecialPos == 0)) {
                if ((sp216 >= QUEST_SONG_MINUET) && (sp216 < QUEST_KOKIRI_EMERALD)) {
                    if (CHECK_QUEST_ITEM(pauseCtx->cursorPoint[PAUSE_QUEST])) {
                        sp216 = pauseCtx->cursorSlot[PAUSE_QUEST];
                        pauseCtx->ocarinaSongIdx = gOcarinaSongItemMap[sp216 - QUEST_SONG_MINUET];
                        D_8082A120 = 10;

                        for (phi_s3 = 0; phi_s3 < 8; phi_s3++) {
                            D_8082A124[phi_s3] = 0xFF;
                            D_8082A150[phi_s3] = 0;
                        }

                        D_8082A11C = 0;
                        AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_DEFAULT);
                        AudioOcarina_Start((1 << pauseCtx->ocarinaSongIdx) + 0x8000);
                        pauseCtx->ocarinaStaff = AudioOcarina_GetPlaybackStaff();
                        pauseCtx->ocarinaStaff->pos = 0;
                        pauseCtx->ocarinaStaff->state = 0xFF;
                        VREG(21) = -62;
                        VREG(22) = -56;
                        VREG(23) = -49;
                        VREG(24) = -46;
                        VREG(25) = -41;
                        pauseCtx->unk_1E4 = 8;
                        AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
                    }
                }
            } else if (pauseCtx->unk_1E4 == 5) {
                if ((pauseCtx->stickRelX != 0) || (pauseCtx->stickRelY != 0)) {
                    pauseCtx->unk_1E4 = 0;
                    AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
                }
            } else if (pauseCtx->unk_1E4 == 8) {
                if (CHECK_BTN_ALL(input->press.button, BTN_A) && (sp216 >= QUEST_SONG_MINUET) &&
                    (sp216 < QUEST_KOKIRI_EMERALD)) {
                    pauseCtx->unk_1E4 = 9;
                    D_8082A120 = 10;
                }
            }
        } else if (pauseCtx->cursorSpecialPos == PAUSE_CURSOR_PAGE_LEFT) {
            if ((pauseCtx->stickRelX > 30)) {
                pauseCtx->cursorPoint[PAUSE_QUEST] = 0x15;
                pauseCtx->nameDisplayTimer = 0;
                pauseCtx->cursorSpecialPos = 0;
                sp216 = pauseCtx->cursorPoint[PAUSE_QUEST];
                KaleidoScope_SetCursorVtx(pauseCtx, sp216 * 4, pauseCtx->questVtx);
                Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                     &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                if (CHECK_QUEST_ITEM(pauseCtx->cursorPoint[PAUSE_QUEST])) {
                    phi_s0_2 = pauseCtx->cursorPoint[PAUSE_QUEST] + 0x5A;
                } else {
                    phi_s0_2 = PAUSE_ITEM_NONE;
                }
                sp216 = pauseCtx->cursorPoint[PAUSE_QUEST];
                pauseCtx->cursorItem[pauseCtx->pageIndex] = phi_s0_2;
                pauseCtx->cursorSlot[pauseCtx->pageIndex] = sp216;
            }
        } else {
            if ((pauseCtx->stickRelX < -30)) {
                pauseCtx->cursorPoint[PAUSE_QUEST] = 0;
                pauseCtx->nameDisplayTimer = 0;
                pauseCtx->cursorSpecialPos = 0;
                sp216 = pauseCtx->cursorPoint[PAUSE_QUEST];
                KaleidoScope_SetCursorVtx(pauseCtx, sp216 * 4, pauseCtx->questVtx);
                Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                     &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                if (CHECK_QUEST_ITEM(pauseCtx->cursorPoint[PAUSE_QUEST])) {
                    if (pauseCtx->cursorPoint[PAUSE_QUEST] < 6) {
                        phi_s0_2 = pauseCtx->cursorPoint[PAUSE_QUEST] + 0x66;
                    } else if (pauseCtx->cursorPoint[PAUSE_QUEST] < 0xC) {
                        phi_s0_2 = pauseCtx->cursorPoint[PAUSE_QUEST] + 0x4E;
                    } else {
                        phi_s0_2 = pauseCtx->cursorPoint[PAUSE_QUEST] + 0x69;
                    }
                } else {
                    phi_s0_2 = PAUSE_ITEM_NONE;
                }
                sp216 = pauseCtx->cursorPoint[PAUSE_QUEST];
                pauseCtx->cursorItem[pauseCtx->pageIndex] = phi_s0_2;
                pauseCtx->cursorSlot[pauseCtx->pageIndex] = sp216;
            }
        }

    } else {
        if (pauseCtx->unk_1E4 == 9) {
            pauseCtx->cursorColorSet = 8;

            if (--D_8082A120 == 0) {
                for (phi_s3 = 0; phi_s3 < 8; phi_s3++) {
                    D_8082A124[phi_s3] = 0xFF;
                    D_8082A150[phi_s3] = 0;
                }

                D_8082A11C = 0;
                VREG(21) = -62;
                VREG(22) = -56;
                VREG(23) = -49;
                VREG(24) = -46;
                VREG(25) = -41;
                sp216 = pauseCtx->cursorSlot[PAUSE_QUEST];
                AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_DEFAULT);
                AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_DEFAULT);
                pauseCtx->ocarinaSongIdx = gOcarinaSongItemMap[sp216 - QUEST_SONG_MINUET];
                AudioOcarina_SetPlaybackSong(pauseCtx->ocarinaSongIdx + 1, 1);
                pauseCtx->unk_1E4 = 2;
                pauseCtx->ocarinaStaff = AudioOcarina_GetPlaybackStaff();
                pauseCtx->ocarinaStaff->pos = 0;
                sp216 = pauseCtx->cursorSlot[PAUSE_QUEST];
                KaleidoScope_SetCursorVtx(pauseCtx, sp216 * 4, pauseCtx->questVtx);
            }
        } else {
            sp216 = pauseCtx->cursorSlot[PAUSE_QUEST];
            KaleidoScope_SetCursorVtx(pauseCtx, sp216 * 4, pauseCtx->questVtx);
        }
    }

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);

    D_8082A0FC--;

    for (sp218 = 0, sp21A = 0; sp218 < 6; sp218++, sp21A += 4) {
        if ((D_8082A100 != 1) && (D_8082A100 != 3)) {
            phi_v1 = (D_8082A100 != 0) ? sp218 + 6 : sp218;

            if (D_8082A0FC != 0) {
                sp226 = ABS(D_8082A0D8[sp218] - D_8082A090[phi_v1][0]) / D_8082A0FC;
                sp224 = ABS(D_8082A0E4[sp218] - D_8082A090[phi_v1][1]) / D_8082A0FC;
                sp222 = ABS(D_8082A0F0[sp218] - D_8082A090[phi_v1][2]) / D_8082A0FC;
                if (D_8082A0D8[sp218] >= D_8082A090[phi_v1][0]) {
                    D_8082A0D8[sp218] -= sp226;
                } else {
                    D_8082A0D8[sp218] += sp226;
                }
                if (D_8082A0E4[sp218] >= D_8082A090[phi_v1][1]) {
                    D_8082A0E4[sp218] -= sp224;
                } else {
                    D_8082A0E4[sp218] += sp224;
                }
                if (D_8082A0F0[sp218] >= D_8082A090[phi_v1][2]) {
                    D_8082A0F0[sp218] -= sp222;
                } else {
                    D_8082A0F0[sp218] += sp222;
                }
            } else {
                D_8082A0D8[sp218] = D_8082A090[phi_v1][0];
                D_8082A0E4[sp218] = D_8082A090[phi_v1][1];
                D_8082A0F0[sp218] = D_8082A090[phi_v1][2];
            }
        }

        if (CHECK_QUEST_ITEM(sp218)) {
            gDPPipeSync(POLY_OPA_DISP++);
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
            gDPSetEnvColor(POLY_OPA_DISP++, D_8082A0D8[sp218], D_8082A0E4[sp218], D_8082A0F0[sp218], 0);
            gSPVertex(POLY_OPA_DISP++, &pauseCtx->questVtx[sp21A], 4, 0);

            KaleidoScope_DrawQuadTextureRGBA32(gfxCtx, gItemIcons[ITEM_MEDALLION_FOREST + sp218], 24, 24, 0);
        }
    }

    if (D_8082A0FC == 0) {
        D_8082A0FC = ZREG(61 + D_8082A100);
        if (++D_8082A100 >= 4) {
            D_8082A100 = 0;
        }
    }

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);

    gDPLoadTextureBlock(POLY_OPA_DISP++, gSongNoteTex, G_IM_FMT_IA, G_IM_SIZ_8b, 16, 24, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

    for (sp218 = 0; sp218 < QUEST_KOKIRI_EMERALD - QUEST_SONG_MINUET; sp218++, sp21A += 4) {
        if (CHECK_QUEST_ITEM(sp218 + QUEST_SONG_MINUET)) {
            if ((sp218 + QUEST_SONG_MINUET) == sp216) {
                pauseCtx->questVtx[sp21A + 0].v.ob[0] = pauseCtx->questVtx[sp21A + 2].v.ob[0] =
                    pauseCtx->questVtx[sp21A + 0].v.ob[0] - 2;

                pauseCtx->questVtx[sp21A + 1].v.ob[0] = pauseCtx->questVtx[sp21A + 3].v.ob[0] =
                    pauseCtx->questVtx[sp21A + 1].v.ob[0] + 4;

                pauseCtx->questVtx[sp21A + 0].v.ob[1] = pauseCtx->questVtx[sp21A + 1].v.ob[1] =
                    pauseCtx->questVtx[sp21A + 0].v.ob[1] + 2;

                pauseCtx->questVtx[sp21A + 2].v.ob[1] = pauseCtx->questVtx[sp21A + 3].v.ob[1] =
                    pauseCtx->questVtx[sp21A + 2].v.ob[1] - 4;
            }

            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, D_8082A164[sp218], D_8082A17C[sp218], D_8082A194[sp218],
                            pauseCtx->alpha);
            gSPVertex(POLY_OPA_DISP++, &pauseCtx->questVtx[sp21A], 4, 0);
            gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
        }
    }

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);

    for (sp218 = 0; sp218 < 3; sp218++, sp21A += 4) {
        if (CHECK_QUEST_ITEM(sp218 + 0x12)) {
            gSPVertex(POLY_OPA_DISP++, &pauseCtx->questVtx[sp21A], 4, 0);
            KaleidoScope_DrawQuadTextureRGBA32(gfxCtx, gItemIcons[ITEM_KOKIRI_EMERALD + sp218], 24, 24, 0);
        }
    }

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);

    for (sp218 = 0; sp218 < 3; sp218++, sp21A += 4) {
        if (CHECK_QUEST_ITEM(sp218 + 0x15)) {
            gSPVertex(POLY_OPA_DISP++, &pauseCtx->questVtx[sp21A], 4, 0);
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
            KaleidoScope_DrawQuadTextureRGBA32(gfxCtx, gItemIcons[ITEM_STONE_OF_AGONY + sp218], 24, 24, 0);
        }
    }

    sp226 = ABS(D_8082A104 - D_8082A070[D_8082A118][0]) / D_8082A114;
    sp224 = ABS(D_8082A108 - D_8082A070[D_8082A118][1]) / D_8082A114;
    sp222 = ABS(D_8082A10C - D_8082A070[D_8082A118][2]) / D_8082A114;
    sp220 = ABS(D_8082A110 - D_8082A070[D_8082A118][3]) / D_8082A114;
    if (D_8082A104 >= D_8082A070[D_8082A118][0]) {
        D_8082A104 -= sp226;
    } else {
        D_8082A104 += sp226;
    }
    if (D_8082A108 >= D_8082A070[D_8082A118][1]) {
        D_8082A108 -= sp224;
    } else {
        D_8082A108 += sp224;
    }
    if (D_8082A10C >= D_8082A070[D_8082A118][2]) {
        D_8082A10C -= sp222;
    } else {
        D_8082A10C += sp222;
    }
    if (D_8082A110 >= D_8082A070[D_8082A118][3]) {
        D_8082A110 -= sp220;
    } else {
        D_8082A110 += sp220;
    }

    if (--D_8082A114 == 0) {
        D_8082A104 = D_8082A070[D_8082A118][0];
        D_8082A108 = D_8082A070[D_8082A118][1];
        D_8082A10C = D_8082A070[D_8082A118][2];
        D_8082A110 = D_8082A070[D_8082A118][3];
        D_8082A114 = ZREG(24 + D_8082A118);
        if (++D_8082A118 >= 4) {
            D_8082A118 = 0;
        }
    }

    if ((gSaveContext.inventory.questItems >> 0x1C) != 0) {
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                          PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);

        if ((pauseCtx->state == 4) || (pauseCtx->state == 0x12)) {
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, D_8082A070[0][0], D_8082A070[0][1], D_8082A070[0][2],
                            pauseCtx->alpha);
        } else {
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, D_8082A104, D_8082A108, D_8082A10C, D_8082A110);
        }

        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
        gSPVertex(POLY_OPA_DISP++, &pauseCtx->questVtx[sp21A], 4, 0);

        POLY_OPA_DISP = KaleidoScope_QuadTextureIA8(
            POLY_OPA_DISP, gItemIcons[0x79 + (((gSaveContext.inventory.questItems & 0xF0000000) & 0xF0000000) >> 0x1C)],
            48, 48, 0);
    }

    if (pauseCtx->state == 6) {
        Color_RGB8 aButtonColor = { 80, 150, 255 };
        if (CVarGetInteger(CVAR_COSMETIC("HUD.AButton.Changed"), 0)) {
            aButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.AButton.Value"), aButtonColor);
        } else if (CVarGetInteger(CVAR_COSMETIC("DefaultColorScheme"), COLORSCHEME_N64) == COLORSCHEME_GAMECUBE) {
            aButtonColor = (Color_RGB8){ 80, 255, 150 };
        }
        if (!GameInteractor_Should(VB_HAVE_OCARINA_NOTE_D4, true)) {
            aButtonColor = (Color_RGB8){ 191, 191, 191 };
        }

        Color_RGB8 cButtonsColor = { 255, 255, 50 };
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CButtons.Changed"), 0)) {
            cButtonsColor = CVarGetColor24(CVAR_COSMETIC("HUD.CButtons.Value"), cButtonsColor);
        }
        Color_RGB8 cUpButtonColor = cButtonsColor;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CUpButton.Changed"), 0)) {
            cUpButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.CUpButton.Value"), cUpButtonColor);
        }
        if (!GameInteractor_Should(VB_HAVE_OCARINA_NOTE_D5, true)) {
            cUpButtonColor = (Color_RGB8){ 191, 191, 191 };
        }

        Color_RGB8 cDownButtonColor = cButtonsColor;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CDownButton.Changed"), 0)) {
            cDownButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.CDownButton.Value"), cDownButtonColor);
        }
        if (!GameInteractor_Should(VB_HAVE_OCARINA_NOTE_F4, true)) {
            cDownButtonColor = (Color_RGB8){ 191, 191, 191 };
        }

        Color_RGB8 cLeftButtonColor = cButtonsColor;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CLeftButton.Changed"), 0)) {
            cLeftButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.CLeftButton.Value"), cLeftButtonColor);
        }
        if (!GameInteractor_Should(VB_HAVE_OCARINA_NOTE_B4, true)) {
            cLeftButtonColor = (Color_RGB8){ 191, 191, 191 };
        }

        Color_RGB8 cRightButtonColor = cButtonsColor;
        if (CVarGetInteger(CVAR_COSMETIC("HUD.CRightButton.Changed"), 0)) {
            cRightButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.CRightButton.Value"), cRightButtonColor);
        }
        if (!GameInteractor_Should(VB_HAVE_OCARINA_NOTE_A4, true)) {
            cRightButtonColor = (Color_RGB8){ 191, 191, 191 };
        }

        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);

        sp21A += 4;
        if ((pauseCtx->cursorSpecialPos == 0) && (sp216 >= 6) && (sp216 < 0x12)) {
            if ((pauseCtx->unk_1E4 < 3) || (pauseCtx->unk_1E4 == 5) || (pauseCtx->unk_1E4 == 8)) {
                if (pauseCtx->cursorItem[pauseCtx->pageIndex] != PAUSE_ITEM_NONE) {
                    pauseCtx->cursorColorSet = 8;
                    if ((pauseCtx->unk_1E4 >= 2) && (pauseCtx->unk_1E4 < 7)) {
                        pauseCtx->cursorColorSet = 0;
                    }
                }
            }
        }

        if (pauseCtx->unk_1E4 == 2) {
            // Draw ocarina buttons as the song playback progresses
            // QUEST_QUAD_SONG_NOTE_A1 to QUEST_QUAD_SONG_NOTE_A8

            pauseCtx->ocarinaStaff = AudioOcarina_GetPlaybackStaff();

            if (pauseCtx->ocarinaStaff->pos != 0) {
                if (D_8082A11C + 1 == pauseCtx->ocarinaStaff->pos) {
                    D_8082A11C++;
                    D_8082A124[pauseCtx->ocarinaStaff->pos - 1] = pauseCtx->ocarinaStaff->buttonIndex;
                }

                for (sp218 = 0, phi_s3 = 0; sp218 < 8; sp218++, phi_s3 += 4, sp21A += 4) {
                    if (D_8082A124[sp218] == 0xFF) {
                        break;
                    }

                    if (D_8082A150[sp218] != 255) {
                        D_8082A150[sp218] += VREG(50);
                        if (D_8082A150[sp218] >= 255) {
                            D_8082A150[sp218] = 255;
                        }
                    }

                    pauseCtx->questVtx[sp21A + 0].v.ob[1] = pauseCtx->questVtx[sp21A + 1].v.ob[1] =
                        VREG(21 + D_8082A124[sp218]);

                    pauseCtx->questVtx[sp21A + 2].v.ob[1] = pauseCtx->questVtx[sp21A + 3].v.ob[1] =
                        pauseCtx->questVtx[sp21A + 0].v.ob[1] - 12;

                    gDPPipeSync(POLY_OPA_DISP++);

                    s16 Notes_alpha = D_8082A150[sp218];
                    if (D_8082A124[sp218] == 0) {
                        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, aButtonColor.r, aButtonColor.g, aButtonColor.b,
                                        Notes_alpha);
                    } else {
                        if (D_8082A124[sp218] == OCARINA_BTN_C_UP) {
                            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, cUpButtonColor.r, cUpButtonColor.g, cUpButtonColor.b,
                                            Notes_alpha);
                        } else if (D_8082A124[sp218] == OCARINA_BTN_C_LEFT) {
                            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, cLeftButtonColor.r, cLeftButtonColor.g,
                                            cLeftButtonColor.b, Notes_alpha);
                        } else if (D_8082A124[sp218] == OCARINA_BTN_C_RIGHT) {
                            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, cRightButtonColor.r, cRightButtonColor.g,
                                            cRightButtonColor.b, Notes_alpha);
                        } else if (D_8082A124[sp218] == OCARINA_BTN_C_DOWN) {
                            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, cDownButtonColor.r, cDownButtonColor.g,
                                            cDownButtonColor.b, Notes_alpha);
                        } else {
                            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, cButtonsColor.r, cButtonsColor.g, cButtonsColor.b,
                                            Notes_alpha);
                        }
                    }

                    gDPSetEnvColor(POLY_OPA_DISP++, 10, 10, 10, 0);
                    gSPVertex(POLY_OPA_DISP++, &pauseCtx->questVtx[sp21A], 4, 0);

                    gDPLoadTextureBlock(POLY_OPA_DISP++, D_8082A130[D_8082A124[sp218]], G_IM_FMT_IA, G_IM_SIZ_8b, 16,
                                        16, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                                        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

                    gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
                }
            }
        } else if (((pauseCtx->unk_1E4 >= 4) && (pauseCtx->unk_1E4 <= 6)) || (pauseCtx->unk_1E4 == 8)) {
            // Draw the buttons for playing a song
            // QUEST_QUAD_SONG_NOTE_A1 to QUEST_QUAD_SONG_NOTE_A8
            sp224 = pauseCtx->ocarinaSongIdx;
            sp226 = gOcarinaSongButtons[sp224].numButtons;

            for (sp218 = sp21A, phi_s3 = 0; phi_s3 < sp226; phi_s3++, sp21A += 4) {
                pauseCtx->questVtx[sp21A + 0].v.ob[1] = pauseCtx->questVtx[sp21A + 1].v.ob[1] =
                    VREG(21 + gOcarinaSongButtons[sp224].buttonsIndex[phi_s3]);

                pauseCtx->questVtx[sp21A + 2].v.ob[1] = pauseCtx->questVtx[sp21A + 3].v.ob[1] =
                    pauseCtx->questVtx[sp21A + 0].v.ob[1] - 12;

                gDPPipeSync(POLY_OPA_DISP++);

                if (pauseCtx->unk_1E4 == 8) {
                    s16 Notes_alpha = 200;
                    if (gOcarinaSongButtons[sp224].buttonsIndex[phi_s3] == 0) {
                        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, aButtonColor.r, aButtonColor.g, aButtonColor.b,
                                        Notes_alpha);
                    } else {
                        if (gOcarinaSongButtons[sp224].buttonsIndex[phi_s3] == OCARINA_BTN_C_UP) {
                            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, cUpButtonColor.r, cUpButtonColor.g, cUpButtonColor.b,
                                            Notes_alpha);
                        } else if (gOcarinaSongButtons[sp224].buttonsIndex[phi_s3] == OCARINA_BTN_C_LEFT) {
                            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, cLeftButtonColor.r, cLeftButtonColor.g,
                                            cLeftButtonColor.b, Notes_alpha);
                        } else if (gOcarinaSongButtons[sp224].buttonsIndex[phi_s3] == OCARINA_BTN_C_RIGHT) {
                            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, cRightButtonColor.r, cRightButtonColor.g,
                                            cRightButtonColor.b, Notes_alpha);
                        } else if (gOcarinaSongButtons[sp224].buttonsIndex[phi_s3] == OCARINA_BTN_C_DOWN) {
                            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, cDownButtonColor.r, cDownButtonColor.g,
                                            cDownButtonColor.b, Notes_alpha);
                        } else {
                            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, cButtonsColor.r, cButtonsColor.g, cButtonsColor.b,
                                            Notes_alpha);
                        }
                    }
                } else {
                    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 150, 150, 150, 150);
                }

                gDPSetEnvColor(POLY_OPA_DISP++, 10, 10, 10, 0);

                gSPVertex(POLY_OPA_DISP++, &pauseCtx->questVtx[sp21A], 4, 0);

                gDPLoadTextureBlock(POLY_OPA_DISP++, D_8082A130[gOcarinaSongButtons[sp224].buttonsIndex[phi_s3]],
                                    G_IM_FMT_IA, G_IM_SIZ_8b, 16, 16, 0, G_TX_NOMIRROR | G_TX_WRAP,
                                    G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

                gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
            }

            if (pauseCtx->unk_1E4 != 8) {
                pauseCtx->ocarinaStaff = AudioOcarina_GetPlayingStaff();

                if (pauseCtx->ocarinaStaff->pos != 0) {
                    if (D_8082A11C == (pauseCtx->ocarinaStaff->pos - 1)) {
                        if ((pauseCtx->ocarinaStaff->buttonIndex >= OCARINA_BTN_A) &&
                            (pauseCtx->ocarinaStaff->buttonIndex <= OCARINA_BTN_C_UP)) {
                            D_8082A124[pauseCtx->ocarinaStaff->pos - 1] = pauseCtx->ocarinaStaff->buttonIndex;
                            D_8082A124[pauseCtx->ocarinaStaff->pos] = 0xFF;
                            D_8082A11C++;
                        }
                    }
                }

                sp21A = sp218 + 32;
                phi_s3 = 0;
                for (; phi_s3 < 8; phi_s3++, sp21A += 4) {
                    if (D_8082A124[phi_s3] == 0xFF) {
                        continue;
                    }

                    if (D_8082A150[phi_s3] != 255) {
                        D_8082A150[phi_s3] += VREG(50);
                        if (D_8082A150[phi_s3] >= 255) {
                            D_8082A150[phi_s3] = 255;
                        }
                    }
                    pauseCtx->questVtx[sp21A + 0].v.ob[1] = pauseCtx->questVtx[sp21A + 1].v.ob[1] =
                        VREG(21 + D_8082A124[phi_s3]);

                    pauseCtx->questVtx[sp21A + 2].v.ob[1] = pauseCtx->questVtx[sp21A + 3].v.ob[1] =
                        pauseCtx->questVtx[sp21A + 0].v.ob[1] - 12;

                    gDPPipeSync(POLY_OPA_DISP++);

                    s16 Notes_alpha = D_8082A150[phi_s3];
                    if (D_8082A124[phi_s3] == 0) {
                        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, aButtonColor.r, aButtonColor.g, aButtonColor.b,
                                        Notes_alpha);
                    } else {
                        if (gOcarinaSongButtons[sp224].buttonsIndex[phi_s3] == OCARINA_BTN_C_UP) {
                            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, cUpButtonColor.r, cUpButtonColor.g, cUpButtonColor.b,
                                            Notes_alpha);
                        } else if (gOcarinaSongButtons[sp224].buttonsIndex[phi_s3] == OCARINA_BTN_C_LEFT) {
                            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, cLeftButtonColor.r, cLeftButtonColor.g,
                                            cLeftButtonColor.b, Notes_alpha);
                        } else if (gOcarinaSongButtons[sp224].buttonsIndex[phi_s3] == OCARINA_BTN_C_RIGHT) {
                            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, cRightButtonColor.r, cRightButtonColor.g,
                                            cRightButtonColor.b, Notes_alpha);
                        } else if (gOcarinaSongButtons[sp224].buttonsIndex[phi_s3] == OCARINA_BTN_C_DOWN) {
                            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, cDownButtonColor.r, cDownButtonColor.g,
                                            cDownButtonColor.b, Notes_alpha);
                        } else {
                            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, cButtonsColor.r, cButtonsColor.g, cButtonsColor.b,
                                            Notes_alpha);
                        }
                    }

                    gDPSetEnvColor(POLY_OPA_DISP++, 10, 10, 10, 0);

                    gSPVertex(POLY_OPA_DISP++, &pauseCtx->questVtx[sp21A], 4, 0);

                    gDPLoadTextureBlock(POLY_OPA_DISP++, D_8082A130[D_8082A124[phi_s3]], G_IM_FMT_IA, G_IM_SIZ_8b, 16,
                                        16, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                                        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

                    gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
                }

                if (pauseCtx->unk_1E4 == 4) {
                    for (phi_s3 = 0; phi_s3 < 8; phi_s3++) {
                        D_8082A124[phi_s3] = 0xFF;
                        D_8082A150[phi_s3] = 0;
                    }

                    D_8082A11C = 0;
                    AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_DEFAULT);
                    AudioOcarina_Start((1 << pauseCtx->ocarinaSongIdx) + 0x8000);
                    pauseCtx->ocarinaStaff = AudioOcarina_GetPlaybackStaff();
                    pauseCtx->ocarinaStaff->pos = 0;
                    pauseCtx->ocarinaStaff->state = 0xFE;
                    pauseCtx->unk_1E4 = 5;
                }
            }
        }
    }

    if (CHECK_QUEST_ITEM(QUEST_SKULL_TOKEN)) {
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                          PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 0);

        sp208[0] = sp208[1] = 0;
        sp208[2] = gSaveContext.inventory.gsTokens;

        while (sp208[2] >= 100) {
            sp208[0]++;
            sp208[2] -= 100;
        }

        while (sp208[2] >= 10) {
            sp208[1]++;
            sp208[2] -= 10;
        }

        gSPVertex(POLY_OPA_DISP++, &pauseCtx->questVtx[164], 24, 0);

        for (phi_s3 = 0, sp218 = 0, sp21A = 0; phi_s3 < 2; phi_s3++) {
            if (phi_s3 == 0) {
                gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 0, 0, 0, pauseCtx->alpha);
            } else if (gSaveContext.inventory.gsTokens == 100) {
                gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 200, 50, 50, pauseCtx->alpha);
            } else {
                gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
            }

            phi_s0 = 0;
            for (sp21A = 0; sp21A < 3; sp21A++, sp218 += 4) {
                if ((sp21A >= 2) || (sp208[sp21A] != 0) || (phi_s0 != 0)) {
                    gDPLoadTextureBlock(POLY_OPA_DISP++, digitTextures[sp208[sp21A]], G_IM_FMT_I, G_IM_SIZ_8b, 8, 16, 0,
                                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                        G_TX_NOLOD, G_TX_NOLOD);

                    gSP1Quadrangle(POLY_OPA_DISP++, sp218, sp218 + 2, sp218 + 3, sp218 + 1, 0);

                    phi_s0 = 1;
                }
            }
        }
    }

    CLOSE_DISPS(gfxCtx);
}

s32 KaleidoScope_UpdateQuestStatusPoint(PauseContext* pauseCtx, s32 point) {
    pauseCtx->cursorPoint[PAUSE_QUEST] = point;

    return 1;
}
