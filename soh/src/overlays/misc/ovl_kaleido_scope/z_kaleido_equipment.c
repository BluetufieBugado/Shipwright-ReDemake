#include "z_kaleido_scope.h"
#include <math.h>
#include "textures/icon_item_static/icon_item_static.h"
#include "textures/parameter_static/parameter_static.h"
#include "soh/Enhancements/cosmetics/cosmeticsTypes.h"
#include "soh/Enhancements/enhancementTypes.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include <libultraship/bridge/consolevariablebridge.h>

static u8 sChildUpgrades[] = { UPG_BULLET_BAG, UPG_BOMB_BAG, UPG_STRENGTH, UPG_SCALE };
static u8 sAdultUpgrades[] = { UPG_QUIVER, UPG_BOMB_BAG, UPG_STRENGTH, UPG_SCALE };

static u8 sChildUpgradeItemBases[] = { ITEM_BULLET_BAG_30, ITEM_BOMB_BAG_20, ITEM_BRACELET, ITEM_SCALE_SILVER };
static u8 sAdultUpgradeItemBases[] = { ITEM_QUIVER_30, ITEM_BOMB_BAG_20, ITEM_BRACELET, ITEM_SCALE_SILVER };

static u8 sUpgradeItemOffsets[] = { 0x00, 0x03, 0x06, 0x09 };

static u8 sEquipmentItemOffsets[] = {
    0x00, 0x00, 0x01, 0x02, 0x00, 0x03, 0x04, 0x05, 0x00, 0x06, 0x07, 0x08, 0x00, 0x09, 0x0A, 0x0B,
};

// Vertices for A button indicator (coordinates 0.75x the texture size)
// pt (-97, -36)
static Vtx sStrengthAButtonVtx[] = {
    VTX(-9, 6, 0, 0 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(9, 6, 0, 24 << 5, 0 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(-9, -6, 0, 0 << 5, 16 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
    VTX(9, -6, 0, 24 << 5, 16 << 5, 0xFF, 0xFF, 0xFF, 0xFF),
};

static s16 sEquipTimer = 0;

extern int gPauseLinkFrameBuffer;

void KaleidoScope_DrawEquipmentImage(PlayState* play, void* source, u32 width, u32 height) {
    PauseContext* pauseCtx = &play->pauseCtx;
    u8* curTexture;
    s32 vtxIndex;
    s32 textureCount;
    s32 textureHeight;
    s32 remainingSize;
    s32 textureSize;
    s32 pad;
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetTextureFilter(POLY_OPA_DISP++, G_TF_POINT);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);

    curTexture = source;
    remainingSize = width * height * 2;
    textureHeight = 4096 / (width * 2);
    textureSize = width * textureHeight * 2;
    textureCount = remainingSize / textureSize;
    if ((remainingSize % textureSize) != 0) {
        textureCount += 1;
    }

    vtxIndex = 80;

    gDPSetTileCustom(POLY_OPA_DISP++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, textureHeight, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                     G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

    remainingSize -= textureSize;

    textureCount = 1;

    // VERTEX Y EXTEND
    pauseCtx->equipVtx[vtxIndex + 2].v.ob[1] -= 80;
    pauseCtx->equipVtx[vtxIndex + 3].v.ob[1] -= 80;

    for (i = 0; i < textureCount; i++) {
        gSPVertex(POLY_OPA_DISP++, &pauseCtx->equipVtx[vtxIndex], 4, 0);

        gDPSetTextureImage(POLY_OPA_DISP++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, curTexture);

        gDPLoadSync(POLY_OPA_DISP++);
        gDPLoadTile(POLY_OPA_DISP++, G_TX_LOADTILE, 0, 0, (width - 1) << 2, (textureHeight - 1) << 2);

        gDPSetTextureImageFB(POLY_OPA_DISP++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, gPauseLinkFrameBuffer);
        gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);

        curTexture += textureSize;

        if ((remainingSize - textureSize) < 0) {
            if (remainingSize > 0) {
                textureHeight = remainingSize / (s32)(width * 2);
                remainingSize -= textureSize;

                gDPSetTileCustom(POLY_OPA_DISP++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, textureHeight, 0,
                                 G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                                 G_TX_NOLOD, G_TX_NOLOD);
            }
        } else {
            remainingSize -= textureSize;
        }

        vtxIndex += 4;
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void KaleidoScope_DrawAButton(PlayState* play, Vtx* vtx, int16_t xTranslate, int16_t yTranslate) {
    PauseContext* pauseCtx = &play->pauseCtx;
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();

    Matrix_Translate(xTranslate, yTranslate, 0, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    Color_RGB8 aButtonColor = { 0, 100, 255 };
    if (CVarGetInteger(CVAR_COSMETIC("HUD.AButton.Changed"), 0)) {
        aButtonColor = CVarGetColor24(CVAR_COSMETIC("HUD.AButton.Value"), aButtonColor);
    } else if (CVarGetInteger(CVAR_COSMETIC("DefaultColorScheme"), COLORSCHEME_N64) == COLORSCHEME_GAMECUBE) {
        aButtonColor = (Color_RGB8){ 0, 255, 100 };
    }

    gSPVertex(POLY_OPA_DISP++, vtx, 4, 0);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, aButtonColor.r, aButtonColor.g, aButtonColor.b, pauseCtx->alpha);
    gDPLoadTextureBlock(POLY_OPA_DISP++, gABtnSymbolTex, G_IM_FMT_IA, G_IM_SIZ_8b, 24, 16, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, 4, 4, G_TX_NOLOD, G_TX_NOLOD);
    gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
    Matrix_Pop();
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    CLOSE_DISPS(play->state.gfxCtx);
}

bool KaleidoScope_IsTabbedEquipPage(PauseContext* pauseCtx);

// Enquadramento do Link na aba Equipamento (menu em abas). Os valores sao deslocamentos sobre a pose base abaixo,
// e a pose atual se aproxima do destino a cada desenho (transicao suave em vez de troca instantanea).
typedef struct {
    f32 dx;       // deslocamento de pos.x
    f32 dy;       // deslocamento de pos.y
    f32 dz;       // deslocamento de pos.z
    f32 scaleMul; // multiplicador de scale
    s16 rotAdd;   // somado a rot.y (0x8000 = de costas)
} EquipLinkFraming;

typedef enum {
    EQUIP_FRAMING_DEFAULT,
    EQUIP_FRAMING_SWORD,  // espada selecionada: gira para mostrar a espada na mao
    EQUIP_FRAMING_SHIELD, // escudo selecionado: gira para mostrar o escudo no braco
    EQUIP_FRAMING_TORSO,  // tunica selecionada: zoom no torso
} EquipLinkFramingId;

// Escala base do Link dentro do oval (o oval e bem maior que o antigo painel). Ajuste ao vivo:
//   set gEnhancements.EquipPose.BaseScale 1.12   set gEnhancements.EquipPose.BaseY 0
#define EQUIP_LINK_BASE_SCALE 1.12f

static EquipLinkFraming sEquipFramings[] = {
    { 0.0f, 0.0f, 0.0f, EQUIP_LINK_BASE_SCALE, 0 },
    { 0.0f, 0.0f, 0.0f, EQUIP_LINK_BASE_SCALE, -0x3000 }, // espada: valores iniciais, ajustar por CVar
    { 0.0f, 0.0f, 0.0f, EQUIP_LINK_BASE_SCALE, 0x3000 },  // escudo: valores iniciais, ajustar por CVar
    { 0.0f, -60.0f, 0.0f, 1.8f, 0 },                      // tunica: valores iniciais, ajustar por CVar
};

static EquipLinkFraming sEquipFramingCur = { 0.0f, 0.0f, 0.0f, EQUIP_LINK_BASE_SCALE, 0 };
static u32 sEquipFramingLastFrame = 0;

static EquipLinkFramingId KaleidoScope_GetEquipLinkFramingId(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    s16 slot;

    if (!KaleidoScope_IsTabbedEquipPage(pauseCtx) || pauseCtx->cursorSpecialPos != 0) {
        return EQUIP_FRAMING_DEFAULT;
    }

    slot = pauseCtx->cursorPoint[PAUSE_EQUIP];
    if ((slot & 3) == 0) { // coluna das melhorias
        return EQUIP_FRAMING_DEFAULT;
    }

    switch (slot >> 2) {
        case 0:
            return EQUIP_FRAMING_SWORD;
        case 1:
            return EQUIP_FRAMING_SHIELD;
        case 2:
            return EQUIP_FRAMING_TORSO;
        default:
            return EQUIP_FRAMING_DEFAULT;
    }
}

// Destino do enquadramento. O zoom da tunica pode ser ajustado ao vivo pelo console:
//   set gEnhancements.EquipPose.TunicY -60   set gEnhancements.EquipPose.TunicScale 1.8
// O giro da espada/escudo (em unidades de angulo do jogo, 0x4000 = 90 graus; trocar o sinal inverte o lado):
//   set gEnhancements.EquipPose.SwordRot -12288   set gEnhancements.EquipPose.ShieldRot 12288
static EquipLinkFraming KaleidoScope_GetEquipLinkFraming(EquipLinkFramingId id) {
    EquipLinkFraming f = sEquipFramings[id];

    if (id != EQUIP_FRAMING_TORSO) {
        f.scaleMul = CVarGetFloat(CVAR_ENHANCEMENT("EquipPose.BaseScale"), f.scaleMul);
        f.dy = CVarGetFloat(CVAR_ENHANCEMENT("EquipPose.BaseY"), f.dy);
    }

    if (id == EQUIP_FRAMING_SWORD) {
        f.rotAdd = (s16)CVarGetInteger(CVAR_ENHANCEMENT("EquipPose.SwordRot"), f.rotAdd);
    } else if (id == EQUIP_FRAMING_SHIELD) {
        f.rotAdd = (s16)CVarGetInteger(CVAR_ENHANCEMENT("EquipPose.ShieldRot"), f.rotAdd);
    } else if (id == EQUIP_FRAMING_TORSO) {
        f.dy = CVarGetFloat(CVAR_ENHANCEMENT("EquipPose.TunicY"), f.dy);
        f.scaleMul = CVarGetFloat(CVAR_ENHANCEMENT("EquipPose.TunicScale"), f.scaleMul);
    }
    return f;
}

static void KaleidoScope_UpdateEquipLinkFraming(PlayState* play) {
    EquipLinkFraming target = KaleidoScope_GetEquipLinkFraming(KaleidoScope_GetEquipLinkFramingId(play));
    EquipLinkFraming* cur = &sEquipFramingCur;

    // Primeira vez nesta visita a pagina (ou fora da pagina por um tempo): comeca ja no destino
    if ((u32)(play->state.frames - sEquipFramingLastFrame) > 2) {
        *cur = target;
    } else {
        Math_SmoothStepToF(&cur->dx, target.dx, 0.3f, 8.0f, 0.05f);
        Math_SmoothStepToF(&cur->dy, target.dy, 0.3f, 8.0f, 0.05f);
        Math_SmoothStepToF(&cur->dz, target.dz, 0.3f, 8.0f, 0.05f);
        Math_SmoothStepToF(&cur->scaleMul, target.scaleMul, 0.3f, 0.2f, 0.001f);
        Math_SmoothStepToS(&cur->rotAdd, target.rotAdd, 4, 0x1000, 0x40);
    }
    sEquipFramingLastFrame = play->state.frames;
}

void KaleidoScope_DrawPlayerWork(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Vec3f pos;
    Vec3s rot;
    f32 scale;

    if (LINK_AGE_IN_YEARS == YEARS_CHILD) {
        pos.x = 2.0f;
        pos.y = -130.0f;
        pos.z = -150.0f;
        scale = 0.046f;
    } else if (CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) != EQUIP_VALUE_SWORD_MASTER &&
               !CVarGetInteger(CVAR_GENERAL("PauseMenuAnimatedLinkTriforce"), 0)) {
        pos.x = 25.0f;
        pos.y = -228.0f;
        pos.z = 60.0f;
        scale = 0.056f;
    } else {
        pos.x = 20.0f;
        pos.y = -180.0f;
        pos.z = -40.0f;
        scale = 0.047f;
    }

    // Menu em abas: aplica o enquadramento (costas / zoom no torso) com transicao suave
    if (KaleidoScope_IsTabbedEquipPage(pauseCtx)) {
        KaleidoScope_UpdateEquipLinkFraming(play);
        pos.x += sEquipFramingCur.dx;
        pos.y += sEquipFramingCur.dy;
        pos.z += sEquipFramingCur.dz;
        scale *= sEquipFramingCur.scaleMul;
    } else {
        sEquipFramingCur = sEquipFramings[EQUIP_FRAMING_DEFAULT];
    }

    // SOH [Port] Draw the pause Link on a separate framebuffer starting in the work buffer
    OPEN_DISPS(play->state.gfxCtx);
    gsSPSetFB(WORK_DISP++, gPauseLinkFrameBuffer);

    rot.y = 32300 + (KaleidoScope_IsTabbedEquipPage(pauseCtx) ? sEquipFramingCur.rotAdd : 0);
    rot.x = rot.z = 0;
    Player_DrawPause(play, pauseCtx->playerSegment, &pauseCtx->playerSkelAnime, &pos, &rot, scale,
                     SWORD_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD)),
                     TUNIC_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC)),
                     SHIELD_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD)),
                     BOOTS_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_BOOTS)));

    gsSPResetFB(WORK_DISP++);
    CLOSE_DISPS(play->state.gfxCtx);
}

// ---------------------------------------------------------------------------------------------------------------
// Menu em abas (CVar gEnhancements.TabbedPauseMenu): layout da aba Equipamento inspirado no remake.
//
// Posicao e tamanho de cada slot ficam na tabela abaixo (unidades locais da pagina, X para a direita, Y para cima).
// O InitVertices (z_kaleido_scope_PAL.c), as molduras e a navegacao leem desta mesma fonte.
// Os indices logicos continuam os do jogo: slot = cursorY * 4 + cursorX
//   cursorY: 0 espadas, 1 escudos, 2 tunicas, 3 botas
//   cursorX: 0 melhoria (bolsa/aljava, bombas, forca, escama), 1..3 equipamento
// Assim a logica de equipar, a posse (gBitFlags) e os botoes C nao mudam.
//
// Disposicao (celulas de 34, moldura de 32, icone de 28):
//   esquerda : coluna A = 4 melhorias, coluna B = 3 botas
//   centro   : painel do Link
//   direita  : espadas / escudos / tunicas (3 x 3)
// ---------------------------------------------------------------------------------------------------------------
#define EQUIP_FRAME_GROW 2        // quanto o slot selecionado cresce por lado
#define EQUIP_DRAW_EMPTY_FRAMES 1 // 1 = desenha moldura tambem nos slots que o jogador ainda nao tem

// Tamanhos por grupo, como no remake: espadas > escudos > tunicas (hw/hh = meia largura/meia altura da moldura;
// icon = lado do icone). Melhorias e botas (coluna da esquerda) usam o mesmo tamanho entre si.
#define EQUIP_SWORD_HW 20
#define EQUIP_SWORD_HH 26
#define EQUIP_SWORD_ICON 42
#define EQUIP_SHIELD_HW 19
#define EQUIP_SHIELD_HH 22
#define EQUIP_SHIELD_ICON 37
#define EQUIP_TUNIC_HW 18
#define EQUIP_TUNIC_HH 21
#define EQUIP_TUNIC_ICON 34
#define EQUIP_LEFT_HW 19
#define EQUIP_LEFT_HH 19
#define EQUIP_LEFT_ICON 34

// Oval do Link (centro, meia largura e meia altura). Proporcao ~0,74 como no remake.
#define EQUIP_OVAL_CX -4
#define EQUIP_OVAL_CY -14
#define EQUIP_OVAL_HW 71
#define EQUIP_OVAL_HH 96
#define EQUIP_OVAL_SEGMENTS 72 // lados do poligono que aproxima a elipse (multiplo de EQUIP_OVAL_CHUNK)
#define EQUIP_OVAL_CHUNK 24    // segmentos por carga de vertices (maximo 30)

// Deslocamento das setas de pagina (L/R), em unidades do painel de informacoes, para fora da grade
#define EQUIP_ARROW_SHIFT_INFO 32

// Centro {x, y} de cada slot, indexado por cursorY * 4 + cursorX
static const s16 sEquipSlotCenter[16][2] = {
    // linha 0: melhoria | espadas
    { -164, 38 }, { 96, 22 }, { 138, 22 }, { 180, 22 },
    // linha 1: melhoria | escudos
    { -164, -4 }, { 96, -30 }, { 138, -30 }, { 180, -30 },
    // linha 2: melhoria | tunicas
    { -164, -46 }, { 96, -79 }, { 138, -79 }, { 180, -79 },
    // linha 3: melhoria | botas (coluna B, a esquerda, intercaladas com as melhorias)
    { -164, -88 }, { -116, 17 }, { -116, -25 }, { -116, -67 },
};

// Tamanho da moldura (meia largura / meia altura) e do icone de um slot
static void KaleidoScope_EquipGetSlotSize(s16 slot, s16* hw, s16* hh, s16* icon) {
    s16 row = slot >> 2;
    s16 col = slot & 3;

    if (col == 0 || row == 3) { // melhorias e botas
        *hw = EQUIP_LEFT_HW;
        *hh = EQUIP_LEFT_HH;
        *icon = EQUIP_LEFT_ICON;
    } else if (row == 0) {
        *hw = EQUIP_SWORD_HW;
        *hh = EQUIP_SWORD_HH;
        *icon = EQUIP_SWORD_ICON;
    } else if (row == 1) {
        *hw = EQUIP_SHIELD_HW;
        *hh = EQUIP_SHIELD_HH;
        *icon = EQUIP_SHIELD_ICON;
    } else {
        *hw = EQUIP_TUNIC_HW;
        *hh = EQUIP_TUNIC_HH;
        *icon = EQUIP_TUNIC_ICON;
    }
}

bool KaleidoScope_IsTabbedEquipPage(PauseContext* pauseCtx) {
    return CVarGetInteger(CVAR_ENHANCEMENT("TabbedPauseMenu"), 1) != 0 && pauseCtx->pageIndex == PAUSE_EQUIP &&
           !((pauseCtx->state >= 8) && (pauseCtx->state <= 0x11));
}

void KaleidoScope_GetEquipSlotCenter(s16 slot, s16* x, s16* y) {
    *x = sEquipSlotCenter[slot][0];
    *y = sEquipSlotCenter[slot][1];
}

s16 KaleidoScope_GetEquipIconSize(void) {
    return EQUIP_LEFT_ICON;
}

s16 KaleidoScope_GetEquipSlotIconSize(s16 slot) {
    s16 hw;
    s16 hh;
    s16 icon;

    KaleidoScope_EquipGetSlotSize(slot, &hw, &hh, &icon);
    return icon;
}

// Retangulo do oval (centro, meia largura, meia altura) em unidades da pagina
void KaleidoScope_GetEquipOvalRect(s16* cx, s16* cy, s16* hw, s16* hh) {
    *cx = EQUIP_OVAL_CX;
    *cy = EQUIP_OVAL_CY;
    *hw = EQUIP_OVAL_HW;
    *hh = EQUIP_OVAL_HH;
}

// Posicao do quadrilatero 80..83 (usado so no layout classico; no layout em abas o oval desenha o Link)
void KaleidoScope_GetEquipLinkImagePos(s16* x, s16* top) {
    *x = -50;
    *top = 51;
}

// Deslocamento horizontal das setas de pagina (esquerda negativo, direita positivo)
s16 KaleidoScope_GetEquipArrowShiftInfo(bool right) {
    return right ? EQUIP_ARROW_SHIFT_INFO : -EQUIP_ARROW_SHIFT_INFO;
}

// O jogador tem o item/melhoria deste slot?
static bool KaleidoScope_EquipSlotOwned(s16 slot) {
    s16 row = slot >> 2;
    s16 col = slot & 3;

    if (col == 0) {
        if (row == 0) {
            return (CUR_UPG_VALUE(UPG_QUIVER) != 0) || (CUR_UPG_VALUE(UPG_BULLET_BAG) != 0);
        }
        return CUR_UPG_VALUE(row) != 0;
    }
    return (gBitFlags[slot - 1] & gSaveContext.inventory.equipment) != 0;
}

static bool KaleidoScope_EquipSlotEquipped(s16 slot) {
    s16 row = slot >> 2;
    s16 col = slot & 3;

    return (col != 0) && (CUR_EQUIP_VALUE(row) == col);
}

// O cursor pode parar neste slot?
static bool KaleidoScope_EquipSlotSelectable(s16 slot, bool pauseAnyCursor) {
    if ((slot & 3) != 0 && pauseAnyCursor) {
        return true;
    }
    return KaleidoScope_EquipSlotOwned(slot);
}

// dir: 0 esquerda, 1 direita, 2 cima, 3 baixo. Devolve o slot selecionavel mais proximo naquela direcao, ou -1.
static s16 KaleidoScope_EquipFindSlotInDir(s16 cur, s32 dir, bool pauseAnyCursor) {
    s16 best = -1;
    s32 bestCost = 0x7FFFFFFF;
    s16 s;

    for (s = 0; s < 16; s++) {
        s32 dx = sEquipSlotCenter[s][0] - sEquipSlotCenter[cur][0];
        s32 dy = sEquipSlotCenter[s][1] - sEquipSlotCenter[cur][1];
        s32 along;
        s32 off;
        s32 cost;

        if (s == cur || !KaleidoScope_EquipSlotSelectable(s, pauseAnyCursor)) {
            continue;
        }

        switch (dir) {
            case 0:
                along = -dx;
                off = (dy < 0) ? -dy : dy;
                break;
            case 1:
                along = dx;
                off = (dy < 0) ? -dy : dy;
                break;
            case 2:
                along = dy;
                off = (dx < 0) ? -dx : dx;
                break;
            default:
                along = -dy;
                off = (dx < 0) ? -dx : dx;
                break;
        }

        if (along < 4 || off > along * 2) {
            continue;
        }

        cost = along + off * 2;
        if (cost < bestCost) {
            bestCost = cost;
            best = s;
        }
    }

    return best;
}

// Entrada vinda de uma seta de pagina: o slot mais proximo da borda (esquerda ou direita), de cima para baixo.
static s16 KaleidoScope_EquipFindEntrySlot(bool fromLeft) {
    s16 best = -1;
    s16 s;

    for (s = 0; s < 16; s++) {
        if (!KaleidoScope_EquipSlotOwned(s)) {
            continue;
        }
        if (best < 0) {
            best = s;
            continue;
        }
        if (fromLeft) {
            if ((sEquipSlotCenter[s][0] < sEquipSlotCenter[best][0]) ||
                ((sEquipSlotCenter[s][0] == sEquipSlotCenter[best][0]) &&
                 (sEquipSlotCenter[s][1] > sEquipSlotCenter[best][1]))) {
                best = s;
            }
        } else {
            if ((sEquipSlotCenter[s][0] > sEquipSlotCenter[best][0]) ||
                ((sEquipSlotCenter[s][0] == sEquipSlotCenter[best][0]) &&
                 (sEquipSlotCenter[s][1] > sEquipSlotCenter[best][1]))) {
                best = s;
            }
        }
    }

    return best;
}

static void KaleidoScope_EquipSetCursorSlot(PauseContext* pauseCtx, s16 slot) {
    pauseCtx->cursorPoint[PAUSE_EQUIP] = slot;
    pauseCtx->cursorX[PAUSE_EQUIP] = slot & 3;
    pauseCtx->cursorY[PAUSE_EQUIP] = slot >> 2;
}

// Navegacao do cursor por posicao na tela (substitui os dois loops de grade 4x4 do jogo original)
static void KaleidoScope_EquipTabbedMoveCursor(PlayState* play, bool dpad, bool pauseAnyCursor) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = &play->state.input[0];
    s16 cur = pauseCtx->cursorPoint[PAUSE_EQUIP];
    s16 next;

    if ((pauseCtx->stickRelX < -30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DLEFT))) {
        next = KaleidoScope_EquipFindSlotInDir(cur, 0, pauseAnyCursor);
        if (next < 0) {
            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_LEFT);
            return;
        }
        cur = next;
    } else if ((pauseCtx->stickRelX > 30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DRIGHT))) {
        next = KaleidoScope_EquipFindSlotInDir(cur, 1, pauseAnyCursor);
        if (next < 0) {
            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_RIGHT);
            return;
        }
        cur = next;
    }

    if ((pauseCtx->stickRelY > 30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DUP))) {
        next = KaleidoScope_EquipFindSlotInDir(cur, 2, pauseAnyCursor);
        if (next >= 0) {
            cur = next;
        }
    } else if ((pauseCtx->stickRelY < -30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DDOWN))) {
        next = KaleidoScope_EquipFindSlotInDir(cur, 3, pauseAnyCursor);
        if (next >= 0) {
            cur = next;
        }
    }

    KaleidoScope_EquipSetCursorSlot(pauseCtx, cur);
}

// Entrada vinda das setas de pagina (fromLeft = veio da seta esquerda).
static void KaleidoScope_EquipTabbedEnter(PlayState* play, bool fromLeft) {
    PauseContext* pauseCtx = &play->pauseCtx;
    s16 slot = KaleidoScope_EquipFindEntrySlot(fromLeft);

    if (slot < 0) {
        KaleidoScope_MoveCursorToSpecialPos(play, fromLeft ? PAUSE_CURSOR_PAGE_RIGHT : PAUSE_CURSOR_PAGE_LEFT);
    } else {
        KaleidoScope_EquipSetCursorSlot(pauseCtx, slot);
    }
}

// Poligono de 8 pontos (retangulo com os cantos cortados), em sentido anti-horario
static void KaleidoScope_EquipDrawPoly(PlayState* play, s16 cx, s16 cy, s16 hw, s16 hh, s16 cut, u8 r, u8 g, u8 b,
                                       u8 a) {
    Vtx* vtx = Graph_Alloc(play->state.gfxCtx, 8 * sizeof(Vtx));
    s16 px[8];
    s16 py[8];
    s32 i;

    if (cut > hw) {
        cut = hw;
    }
    if (cut > hh) {
        cut = hh;
    }

    px[0] = cx - hw + cut; py[0] = cy + hh;
    px[1] = cx - hw;       py[1] = cy + hh - cut;
    px[2] = cx - hw;       py[2] = cy - hh + cut;
    px[3] = cx - hw + cut; py[3] = cy - hh;
    px[4] = cx + hw - cut; py[4] = cy - hh;
    px[5] = cx + hw;       py[5] = cy - hh + cut;
    px[6] = cx + hw;       py[6] = cy + hh - cut;
    px[7] = cx + hw - cut; py[7] = cy + hh;

    for (i = 0; i < 8; i++) {
        vtx[i].v.ob[0] = px[i];
        vtx[i].v.ob[1] = py[i];
        vtx[i].v.ob[2] = 0;
        vtx[i].v.flag = 0;
        vtx[i].v.tc[0] = 0;
        vtx[i].v.tc[1] = 0;
        vtx[i].v.cn[0] = 255;
        vtx[i].v.cn[1] = 255;
        vtx[i].v.cn[2] = 255;
        vtx[i].v.cn[3] = 255;
    }

    OPEN_DISPS(play->state.gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, r, g, b, a);
    gSPVertex(POLY_OPA_DISP++, vtx, 8, 0);
    gSP2Triangles(POLY_OPA_DISP++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSP2Triangles(POLY_OPA_DISP++, 0, 3, 4, 0, 0, 4, 5, 0);
    gSP2Triangles(POLY_OPA_DISP++, 0, 5, 6, 0, 0, 6, 7, 0);

    CLOSE_DISPS(play->state.gfxCtx);
}

// Moldura de um slot: borda + miolo
static void KaleidoScope_EquipDrawFrame(PlayState* play, s16 cx, s16 cy, s16 hw, s16 hh, bool selected,
                                        bool equipped, bool owned, u8 alpha) {
    s16 cut = (hw < hh ? hw : hh) / 2 - 3; // corte dos cantos (octogono) proporcional ao tamanho
    if (cut < 5) {
        cut = 5;
    }
    u8 br, bg, bb; // borda
    u8 fr, fg, fb; // miolo

    if (selected) {
        br = 255; bg = 250; bb = 230;
        if (equipped) {
            fr = 176; fg = 128; fb = 24;
        } else {
            fr = 70; fg = 54; fb = 34;
        }
    } else if (equipped) {
        br = 214; bg = 170; bb = 70;
        fr = 176; fg = 128; fb = 24;
    } else if (owned) {
        br = 96; bg = 76; bb = 50;
        fr = 22; fg = 17; fb = 13;
    } else {
        br = 62; bg = 49; bb = 34;
        fr = 14; fg = 11; fb = 9;
    }

    KaleidoScope_EquipDrawPoly(play, cx, cy, hw, hh, cut, br, bg, bb, alpha);
    KaleidoScope_EquipDrawPoly(play, cx, cy, hw - 2, hh - 2, cut - 1, fr, fg, fb, alpha);
}

// Painel do Link e molduras dos slots, desenhados antes dos icones (mesma matriz dos icones, entao acompanham
// o deslocamento da pagina e a animacao de abrir/fechar).
static void KaleidoScope_DrawEquipFrames(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    u8 alpha = (pauseCtx->alpha > 255) ? 255 : (u8)pauseCtx->alpha;
    s16 selectedSlot = (pauseCtx->cursorSpecialPos == 0) ? pauseCtx->cursorPoint[PAUSE_EQUIP] : -1;
    s16 hw;
    s16 hh;
    s16 icon;
    s16 slot;
    s16 x;
    s16 y;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_42Opa(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);

    // (O painel do Link agora e o oval, desenhado por KaleidoScope_DrawEquipOvalImage)

    // Molduras (o slot selecionado por ultimo, para ficar por cima)
    for (slot = 0; slot < 16; slot++) {
        bool owned = KaleidoScope_EquipSlotOwned(slot);

        if (slot == selectedSlot || (!owned && !EQUIP_DRAW_EMPTY_FRAMES)) {
            continue;
        }

        KaleidoScope_GetEquipSlotCenter(slot, &x, &y);
        KaleidoScope_EquipGetSlotSize(slot, &hw, &hh, &icon);
        KaleidoScope_EquipDrawFrame(play, x, y + pauseCtx->offsetY, hw, hh, false,
                                    KaleidoScope_EquipSlotEquipped(slot), owned, alpha);
    }

    if (selectedSlot >= 0) {
        KaleidoScope_GetEquipSlotCenter(selectedSlot, &x, &y);
        KaleidoScope_EquipGetSlotSize(selectedSlot, &hw, &hh, &icon);
        KaleidoScope_EquipDrawFrame(play, x, y + pauseCtx->offsetY, hw + EQUIP_FRAME_GROW, hh + EQUIP_FRAME_GROW, true,
                                    KaleidoScope_EquipSlotEquipped(selectedSlot), true, alpha);
    }

    // Restaura o estado grafico esperado pelo resto da pagina
    gDPPipeSync(POLY_OPA_DISP++);
    Gfx_SetupDL_42Opa(play->state.gfxCtx);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);

    CLOSE_DISPS(play->state.gfxCtx);
}

// Preenche a elipse com a textura do Link (framebuffer): leque de triangulos com o centro + pontos da borda.
// Cada ponto recebe a coordenada de textura correspondente a sua posicao na elipse, entao o Link e recortado no oval
// sem precisar de scissor. As coordenadas de textura seguem a mesma convencao do quadrilatero original
// (largura inteira = W << 5, altura do tile = 4096 / (W * 2) linhas, em s10.5).
static void KaleidoScope_EquipDrawOvalFill(PlayState* play, s16 cx, s16 cy, s16 hw, s16 hh) {
    const s32 chunks = EQUIP_OVAL_SEGMENTS / EQUIP_OVAL_CHUNK;
    const f32 texW = (f32)(PAUSE_EQUIP_PLAYER_WIDTH << 5);
    const f32 texH = (f32)((4096 / (PAUSE_EQUIP_PLAYER_WIDTH * 2)) << 5);
    // Recuo da amostragem nas bordas do framebuffer. Com filtro bilinear, amostrar perto da borda le texels vizinhos
    // que "dao a volta" e mostram o topo do Link (o pescoco) na base do oval. Um recuo de ~1,5 texel nao bastou
    // (o vazamento tem varios texels), entao o padrao agora e 8 texels (~3% da altura). O recuo e uniforme em U e V
    // (a proporcao do oval e ~3:4, a do framebuffer), entao o Link so aparece ~3% maior, sem distorcer.
    // Ajustavel ao vivo pela CVar gEnhancements.EquipPose.OvalInset (em texels do framebuffer).
    const f32 insetTexels = CLAMP(CVarGetFloat(CVAR_ENHANCEMENT("EquipPose.OvalInset"), 8.0f), 0.0f, 40.0f);
    const f32 insetU = 1.0f - insetTexels / (f32)PAUSE_EQUIP_PLAYER_HEIGHT;
    const f32 insetV = insetU;
    s32 chunk;
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);

    for (chunk = 0; chunk < chunks; chunk++) {
        Vtx* vtx = Graph_Alloc(play->state.gfxCtx, (EQUIP_OVAL_CHUNK + 2) * sizeof(Vtx));

        // vertice 0: centro
        vtx[0].v.ob[0] = cx;
        vtx[0].v.ob[1] = cy;
        vtx[0].v.ob[2] = 0;
        vtx[0].v.flag = 0;
        vtx[0].v.tc[0] = (s16)(texW * 0.5f);
        vtx[0].v.tc[1] = (s16)(texH * 0.5f);
        vtx[0].v.cn[0] = vtx[0].v.cn[1] = vtx[0].v.cn[2] = vtx[0].v.cn[3] = 255;

        for (i = 0; i <= EQUIP_OVAL_CHUNK; i++) {
            f32 ang = (f32)(chunk * EQUIP_OVAL_CHUNK + i) * (6.2831853f / (f32)EQUIP_OVAL_SEGMENTS);
            f32 ca = cosf(ang);
            f32 sa = sinf(ang);
            Vtx* v = &vtx[1 + i];

            v->v.ob[0] = (s16)(cx + hw * ca + (ca >= 0.0f ? 0.5f : -0.5f));
            v->v.ob[1] = (s16)(cy + hh * sa + (sa >= 0.0f ? 0.5f : -0.5f));
            v->v.ob[2] = 0;
            v->v.flag = 0;
            v->v.tc[0] = (s16)(texW * (0.5f + 0.5f * ca * insetU));
            v->v.tc[1] = (s16)(texH * (0.5f - 0.5f * sa * insetV));
            v->v.cn[0] = v->v.cn[1] = v->v.cn[2] = v->v.cn[3] = 255;
        }

        gSPVertex(POLY_OPA_DISP++, vtx, EQUIP_OVAL_CHUNK + 2, 0);
        for (i = 0; i < EQUIP_OVAL_CHUNK; i += 2) {
            gSP2Triangles(POLY_OPA_DISP++, 0, 1 + i, 2 + i, 0, 0, 2 + i, 3 + i, 0);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// Anel entre duas elipses concentricas (externa: hwO/hhO, interna: hwI/hhI), cor lisa.
#define EQUIP_RING_CHUNK 12
static void KaleidoScope_EquipDrawOvalRing(PlayState* play, s16 cx, s16 cy, s16 hwO, s16 hhO, s16 hwI, s16 hhI, u8 r,
                                           u8 g, u8 b, u8 a) {
    const s32 chunks = EQUIP_OVAL_SEGMENTS / EQUIP_RING_CHUNK;
    s32 chunk;
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, r, g, b, a);

    for (chunk = 0; chunk < chunks; chunk++) {
        Vtx* vtx = Graph_Alloc(play->state.gfxCtx, (EQUIP_RING_CHUNK + 1) * 2 * sizeof(Vtx));

        for (i = 0; i <= EQUIP_RING_CHUNK; i++) {
            f32 ang = (f32)(chunk * EQUIP_RING_CHUNK + i) * (6.2831853f / (f32)EQUIP_OVAL_SEGMENTS);
            f32 ca = cosf(ang);
            f32 sa = sinf(ang);
            Vtx* vo = &vtx[i * 2];     // ponto externo
            Vtx* vi = &vtx[i * 2 + 1]; // ponto interno

            vo->v.ob[0] = (s16)(cx + hwO * ca + (ca >= 0.0f ? 0.5f : -0.5f));
            vo->v.ob[1] = (s16)(cy + hhO * sa + (sa >= 0.0f ? 0.5f : -0.5f));
            vi->v.ob[0] = (s16)(cx + hwI * ca + (ca >= 0.0f ? 0.5f : -0.5f));
            vi->v.ob[1] = (s16)(cy + hhI * sa + (sa >= 0.0f ? 0.5f : -0.5f));
            vo->v.ob[2] = vi->v.ob[2] = 0;
            vo->v.flag = vi->v.flag = 0;
            vo->v.tc[0] = vo->v.tc[1] = vi->v.tc[0] = vi->v.tc[1] = 0;
            vo->v.cn[0] = vo->v.cn[1] = vo->v.cn[2] = vo->v.cn[3] = 255;
            vi->v.cn[0] = vi->v.cn[1] = vi->v.cn[2] = vi->v.cn[3] = 255;
        }

        gSPVertex(POLY_OPA_DISP++, vtx, (EQUIP_RING_CHUNK + 1) * 2, 0);
        for (i = 0; i < EQUIP_RING_CHUNK; i++) {
            // vertices: externo i = 2i, interno i = 2i+1, externo i+1 = 2i+2, interno i+1 = 2i+3 (sentido anti-horario)
            gSP2Triangles(POLY_OPA_DISP++, 2 * i, 2 * i + 2, 2 * i + 3, 0, 2 * i, 2 * i + 3, 2 * i + 1, 0);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// Moldura oval do Link (menu em abas): a textura do framebuffer do Link recortada na elipse + anel dourado/marrom.
static void KaleidoScope_DrawEquipOvalImage(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    u8 alpha = (pauseCtx->alpha > 255) ? 255 : (u8)pauseCtx->alpha;
    s16 cx = EQUIP_OVAL_CX;
    s16 cy = EQUIP_OVAL_CY + pauseCtx->offsetY;
    const s32 width = PAUSE_EQUIP_PLAYER_WIDTH;
    const s32 textureHeight = 4096 / (width * 2);

    OPEN_DISPS(play->state.gfxCtx);

    // 1) Link: mesma sequencia de DrawEquipmentImage (tile + framebuffer), mas com filtro bilinear, porque o
    // oval e bem maior que o quadro antigo.
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetTextureFilter(POLY_OPA_DISP++, G_TF_BILERP);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, alpha);

    gDPSetTileCustom(POLY_OPA_DISP++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, textureHeight, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                     G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPSetTextureImage(POLY_OPA_DISP++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, pauseCtx->playerSegment);
    gDPLoadSync(POLY_OPA_DISP++);
    gDPLoadTile(POLY_OPA_DISP++, G_TX_LOADTILE, 0, 0, (width - 1) << 2, (textureHeight - 1) << 2);
    gDPSetTextureImageFB(POLY_OPA_DISP++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, gPauseLinkFrameBuffer);

    KaleidoScope_EquipDrawOvalFill(play, cx, cy, EQUIP_OVAL_HW, EQUIP_OVAL_HH);

    // 2) Aneis (cor lisa): sombra escura por fora e linha dourada por cima da borda da imagem
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    KaleidoScope_EquipDrawOvalRing(play, cx, cy, EQUIP_OVAL_HW + 6, EQUIP_OVAL_HH + 6, EQUIP_OVAL_HW + 2,
                                   EQUIP_OVAL_HH + 2, 38, 29, 20, alpha);
    KaleidoScope_EquipDrawOvalRing(play, cx, cy, EQUIP_OVAL_HW + 3, EQUIP_OVAL_HH + 3, EQUIP_OVAL_HW - 1,
                                   EQUIP_OVAL_HH - 1, 164, 128, 74, alpha);

    // Restaura o estado grafico esperado pelo resto da pagina
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetTextureFilter(POLY_OPA_DISP++, G_TF_POINT);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);

    CLOSE_DISPS(play->state.gfxCtx);
}

void KaleidoScope_DrawEquipment(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = &play->state.input[0];
    u16 i;
    u16 j;
    u16 k;
    u16 bit;
    u16 temp;
    u16 point;
    u16 rowStart;
    u16 pad;
    s16 cursorMoveResult;
    u16 cursorItem;
    u16 cursorSlot = 0;
    s16 cursorPoint;
    s16 cursorX;
    s16 cursorY;
    s16 oldCursorPoint;
    bool tabbedLayout = KaleidoScope_IsTabbedEquipPage(pauseCtx);

    OPEN_DISPS(play->state.gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, ZREG(39), ZREG(40), ZREG(41), pauseCtx->alpha);
    gDPSetEnvColor(POLY_OPA_DISP++, ZREG(43), ZREG(44), ZREG(45), 0);

    for (i = 0, j = 64; i < 4; i++, j += 4) {
        if ((CUR_EQUIP_VALUE(i) != 0) && !tabbedLayout) {
            gDPPipeSync(POLY_OPA_DISP++);
            gSPVertex(POLY_OPA_DISP++, &pauseCtx->equipVtx[j], 4, 0);

            POLY_OPA_DISP = KaleidoScope_QuadTextureIA8(POLY_OPA_DISP, gEquippedItemOutlineTex, 32, 32, 0);
        }
    }

    if ((pauseCtx->state == 6) && (pauseCtx->unk_1E4 == 0) && (pauseCtx->pageIndex == PAUSE_EQUIP)) {
        bool dpad = (CVarGetInteger(CVAR_SETTING("DPadOnPause"), 0) && !CHECK_BTN_ALL(input->cur.button, BTN_CUP));
        bool pauseAnyCursor =
            (CVarGetInteger(CVAR_ENHANCEMENT("PauseAnyCursor"), 0) == PAUSE_ANY_CURSOR_RANDO_ONLY && IS_RANDO) ||
            (CVarGetInteger(CVAR_ENHANCEMENT("PauseAnyCursor"), 0) == PAUSE_ANY_CURSOR_ALWAYS_ON);

        oldCursorPoint = pauseCtx->cursorPoint[PAUSE_EQUIP];
        pauseCtx->cursorColorSet = 0;

        if (pauseCtx->cursorSpecialPos == 0) {
            pauseCtx->nameColorSet = 0;

            cursorItem = pauseCtx->cursorItem[PAUSE_EQUIP];
            if ((cursorItem >= ITEM_SWORD_KOKIRI) && (cursorItem <= ITEM_BOOTS_HOVER)) {
                pauseCtx->cursorColorSet = 8;
            }

            cursorPoint = pauseCtx->cursorPoint[PAUSE_EQUIP];
            cursorX = pauseCtx->cursorX[PAUSE_EQUIP];
            cursorY = pauseCtx->cursorY[PAUSE_EQUIP];

            cursorMoveResult = 0;
            if (tabbedLayout) {
                KaleidoScope_EquipTabbedMoveCursor(play, dpad, pauseAnyCursor);
                cursorMoveResult = 4;
            }
            while (cursorMoveResult == 0) {
                if ((pauseCtx->stickRelX < -30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DLEFT))) {
                    if (pauseCtx->cursorX[PAUSE_EQUIP] != 0) {
                        pauseCtx->cursorX[PAUSE_EQUIP] -= 1;
                        pauseCtx->cursorPoint[PAUSE_EQUIP] -= 1;

                        if (pauseCtx->cursorX[PAUSE_EQUIP] == 0) {
                            if (pauseCtx->cursorY[PAUSE_EQUIP] == 0) {
                                if (CUR_UPG_VALUE(UPG_BULLET_BAG) != 0) {
                                    cursorMoveResult = 1;
                                }
                            } else {
                                if (CUR_UPG_VALUE(pauseCtx->cursorY[PAUSE_EQUIP]) != 0) {
                                    cursorMoveResult = 1;
                                }
                            }
                        } else if ((gBitFlags[pauseCtx->cursorPoint[PAUSE_EQUIP] - 1] &
                                    gSaveContext.inventory.equipment) ||
                                   pauseAnyCursor) {
                            cursorMoveResult = 2;
                        }
                    } else {
                        pauseCtx->cursorX[PAUSE_EQUIP] = cursorX;
                        pauseCtx->cursorY[PAUSE_EQUIP] += 1;

                        if (pauseCtx->cursorY[PAUSE_EQUIP] >= 4) {
                            pauseCtx->cursorY[PAUSE_EQUIP] = 0;
                        }

                        pauseCtx->cursorPoint[PAUSE_EQUIP] =
                            pauseCtx->cursorX[PAUSE_EQUIP] + (pauseCtx->cursorY[PAUSE_EQUIP] * 4);

                        if (pauseCtx->cursorPoint[PAUSE_EQUIP] >= 16) {
                            pauseCtx->cursorPoint[PAUSE_EQUIP] = pauseCtx->cursorX[PAUSE_EQUIP];
                        }

                        if (cursorY == pauseCtx->cursorY[PAUSE_EQUIP]) {
                            pauseCtx->cursorX[PAUSE_EQUIP] = cursorX;
                            pauseCtx->cursorPoint[PAUSE_EQUIP] = cursorPoint;
                            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_LEFT);
                            cursorMoveResult = 3;
                        }
                    }
                } else if ((pauseCtx->stickRelX > 30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DRIGHT))) {
                    if (pauseCtx->cursorX[PAUSE_EQUIP] < 3) {
                        pauseCtx->cursorX[PAUSE_EQUIP] += 1;
                        pauseCtx->cursorPoint[PAUSE_EQUIP] += 1;

                        if (pauseCtx->cursorX[PAUSE_EQUIP] == 0) {
                            if (CUR_UPG_VALUE(pauseCtx->cursorY[PAUSE_EQUIP]) != 0) {
                                cursorMoveResult = 1;
                            }
                        } else if ((gBitFlags[pauseCtx->cursorPoint[PAUSE_EQUIP] - 1] &
                                    gSaveContext.inventory.equipment) ||
                                   pauseAnyCursor) {
                            cursorMoveResult = 2;
                        }
                    } else {
                        pauseCtx->cursorX[PAUSE_EQUIP] = cursorX;
                        pauseCtx->cursorY[PAUSE_EQUIP] += 1;

                        if (pauseCtx->cursorY[PAUSE_EQUIP] >= 4) {
                            pauseCtx->cursorY[PAUSE_EQUIP] = 0;
                        }

                        pauseCtx->cursorPoint[PAUSE_EQUIP] =
                            pauseCtx->cursorX[PAUSE_EQUIP] + (pauseCtx->cursorY[PAUSE_EQUIP] * 4);

                        if (pauseCtx->cursorPoint[PAUSE_EQUIP] >= 16) {
                            pauseCtx->cursorPoint[PAUSE_EQUIP] = pauseCtx->cursorX[PAUSE_EQUIP];
                        }

                        if (cursorY == pauseCtx->cursorY[PAUSE_EQUIP]) {
                            pauseCtx->cursorX[PAUSE_EQUIP] = cursorX;
                            pauseCtx->cursorPoint[PAUSE_EQUIP] = cursorPoint;
                            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_RIGHT);
                            cursorMoveResult = 3;
                        }
                    }
                } else {
                    cursorMoveResult = 4;
                }
            }

            cursorPoint = pauseCtx->cursorPoint[PAUSE_EQUIP];
            cursorY = pauseCtx->cursorY[PAUSE_EQUIP];

            if (cursorMoveResult) {}

            cursorMoveResult = tabbedLayout ? 4 : 0;
            while (cursorMoveResult == 0) {
                if ((pauseCtx->stickRelY > 30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DUP))) {
                    if (pauseCtx->cursorY[PAUSE_EQUIP] != 0) {
                        pauseCtx->cursorY[PAUSE_EQUIP] -= 1;
                        pauseCtx->cursorPoint[PAUSE_EQUIP] -= 4;

                        if (pauseCtx->cursorX[PAUSE_EQUIP] == 0) {
                            if (pauseCtx->cursorY[PAUSE_EQUIP] == 0) {
                                if (CUR_UPG_VALUE(UPG_BULLET_BAG) != 0) {
                                    cursorMoveResult = 1;
                                }
                            } else if (CUR_UPG_VALUE(pauseCtx->cursorY[PAUSE_EQUIP]) != 0) {
                                cursorMoveResult = 1;
                            }
                        } else if ((gBitFlags[pauseCtx->cursorPoint[PAUSE_EQUIP] - 1] &
                                    gSaveContext.inventory.equipment) ||
                                   pauseAnyCursor) {
                            cursorMoveResult = 2;
                        }
                    } else {
                        pauseCtx->cursorY[PAUSE_EQUIP] = cursorY;
                        pauseCtx->cursorPoint[PAUSE_EQUIP] = cursorPoint;
                        cursorMoveResult = 3;
                    }
                } else if ((pauseCtx->stickRelY < -30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DDOWN))) {
                    if (pauseCtx->cursorY[PAUSE_EQUIP] < 3) {
                        pauseCtx->cursorY[PAUSE_EQUIP] += 1;
                        pauseCtx->cursorPoint[PAUSE_EQUIP] += 4;

                        if (pauseCtx->cursorX[PAUSE_EQUIP] == 0) {
                            if (CUR_UPG_VALUE(pauseCtx->cursorY[PAUSE_EQUIP]) != 0) {
                                cursorMoveResult = 1;
                            }
                        } else if ((gBitFlags[pauseCtx->cursorPoint[PAUSE_EQUIP] - 1] &
                                    gSaveContext.inventory.equipment) ||
                                   pauseAnyCursor) {
                            cursorMoveResult = 2;
                        }
                    } else {
                        pauseCtx->cursorY[PAUSE_EQUIP] = cursorY;
                        pauseCtx->cursorPoint[PAUSE_EQUIP] = cursorPoint;
                        cursorMoveResult = 3;
                    }
                } else {
                    cursorMoveResult = 4;
                }
            }
        } else if (pauseCtx->cursorSpecialPos == PAUSE_CURSOR_PAGE_LEFT) {
            if ((pauseCtx->stickRelX > 30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DRIGHT))) {
                pauseCtx->nameDisplayTimer = 0;
                pauseCtx->cursorSpecialPos = 0;

                Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                     &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);

                cursorPoint = cursorX = cursorY = 0;
                if (tabbedLayout) {
                    KaleidoScope_EquipTabbedEnter(play, true);
                }
                while (!tabbedLayout) {
                    if (cursorX == 0) {
                        if (cursorY == 0) {
                            if (CUR_UPG_VALUE(UPG_BULLET_BAG) != 0) {
                                pauseCtx->cursorPoint[PAUSE_EQUIP] = cursorPoint;
                                pauseCtx->cursorX[PAUSE_EQUIP] = cursorX;
                                pauseCtx->cursorY[PAUSE_EQUIP] = cursorY;
                                break;
                            }
                        } else if (CUR_UPG_VALUE(cursorY) != 0) {
                            pauseCtx->cursorPoint[PAUSE_EQUIP] = cursorPoint;
                            pauseCtx->cursorX[PAUSE_EQUIP] = cursorX;
                            pauseCtx->cursorY[PAUSE_EQUIP] = cursorY;
                            break;
                        }
                    } else if (gBitFlags[cursorPoint - 1] & gSaveContext.inventory.equipment) {
                        pauseCtx->cursorPoint[PAUSE_EQUIP] = cursorPoint;
                        pauseCtx->cursorX[PAUSE_EQUIP] = cursorX;
                        pauseCtx->cursorY[PAUSE_EQUIP] = cursorY;
                        break;
                    }

                    cursorY = cursorY + 1;
                    cursorPoint = cursorPoint + 4;
                    if (cursorY < 4) {
                        continue;
                    }

                    cursorY = 0;
                    cursorPoint = cursorX + 1;
                    cursorX = cursorPoint;
                    if (cursorX < 4) {
                        continue;
                    }

                    KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_RIGHT);
                    break;
                }
            }
        } else {
            if ((pauseCtx->stickRelX < -30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DLEFT))) {
                pauseCtx->nameDisplayTimer = 0;
                pauseCtx->cursorSpecialPos = 0;
                Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                     &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);

                cursorPoint = cursorX = 3;
                cursorY = 0;
                if (tabbedLayout) {
                    KaleidoScope_EquipTabbedEnter(play, false);
                }
                while (!tabbedLayout) {
                    if (cursorX == 0) {
                        if (CUR_UPG_VALUE(cursorY) != 0) {
                            pauseCtx->cursorPoint[PAUSE_EQUIP] = cursorPoint;
                            pauseCtx->cursorX[PAUSE_EQUIP] = cursorX;
                            pauseCtx->cursorY[PAUSE_EQUIP] = cursorY;
                            break;
                        }
                    } else if (gBitFlags[cursorPoint - 1] & gSaveContext.inventory.equipment) {
                        pauseCtx->cursorPoint[PAUSE_EQUIP] = cursorPoint;
                        pauseCtx->cursorX[PAUSE_EQUIP] = cursorX;
                        pauseCtx->cursorY[PAUSE_EQUIP] = cursorY;
                        break;
                    }

                    cursorY = cursorY + 1;
                    cursorPoint = cursorPoint + 4;
                    if (cursorY < 4) {
                        continue;
                    }

                    cursorY = 0;
                    cursorPoint = cursorX - 1;
                    cursorX = cursorPoint;
                    if (cursorX >= 0) {
                        continue;
                    }

                    KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_LEFT);
                    break;
                }
            }
        }

        if (pauseCtx->cursorX[PAUSE_EQUIP] == 0) {
            pauseCtx->cursorColorSet = 0;

            if (LINK_AGE_IN_YEARS == YEARS_CHILD) {
                if ((pauseCtx->cursorY[PAUSE_EQUIP] == 0) && (CUR_UPG_VALUE(UPG_BULLET_BAG) != 0)) {
                    cursorItem = ITEM_BULLET_BAG_30 + CUR_UPG_VALUE(UPG_BULLET_BAG) - 1;
                } else {
                    cursorItem = ITEM_QUIVER_30 + sUpgradeItemOffsets[pauseCtx->cursorY[PAUSE_EQUIP]] +
                                 CUR_UPG_VALUE(pauseCtx->cursorY[PAUSE_EQUIP]) - 1;
                    osSyncPrintf("H_arrowcase_1 + non_equip_item_table = %d\n", cursorItem);
                }
            } else {
                if ((pauseCtx->cursorY[PAUSE_EQUIP] == 0) && (CUR_UPG_VALUE(UPG_QUIVER) == 0)) {
                    cursorItem = ITEM_BULLET_BAG_30 + CUR_UPG_VALUE(UPG_BULLET_BAG) - 1;
                } else {
                    cursorItem = ITEM_QUIVER_30 + sUpgradeItemOffsets[pauseCtx->cursorY[PAUSE_EQUIP]] +
                                 CUR_UPG_VALUE(pauseCtx->cursorY[PAUSE_EQUIP]) - 1;
                    osSyncPrintf("大人 H_arrowcase_1 + non_equip_item_table = %d\n", cursorItem);
                }
            }
        } else {
            cursorItem = ITEM_SWORD_KOKIRI + sEquipmentItemOffsets[pauseCtx->cursorPoint[PAUSE_EQUIP]];
            osSyncPrintf("ccc=%d\n", cursorItem);

            if (pauseCtx->cursorSpecialPos == 0) {
                pauseCtx->cursorColorSet = 8;
            }
        }

        if ((pauseCtx->cursorY[PAUSE_EQUIP] == 0) && (pauseCtx->cursorX[PAUSE_EQUIP] == 3)) {
            if (gSaveContext.bgsFlag != 0) {
                cursorItem = ITEM_HEART_PIECE_2;
            } else if (CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE)) {
                cursorItem = ITEM_SWORD_KNIFE;
            }
        }

        cursorSlot = pauseCtx->cursorPoint[PAUSE_EQUIP];

        pauseCtx->cursorItem[PAUSE_EQUIP] = cursorItem;
        pauseCtx->cursorSlot[PAUSE_EQUIP] = cursorSlot;

        osSyncPrintf("kscope->select_name[Display_Equipment] = %d\n", pauseCtx->cursorItem[PAUSE_EQUIP]);

        if (!(CHECK_AGE_REQ_EQUIP(pauseCtx->cursorY[PAUSE_EQUIP], pauseCtx->cursorX[PAUSE_EQUIP]))) {
            pauseCtx->nameColorSet = 1;
        }

        if (pauseCtx->cursorItem[PAUSE_EQUIP] == ITEM_BRACELET) {
            if (LINK_AGE_IN_YEARS == YEARS_CHILD || IS_RANDO) {
                pauseCtx->nameColorSet = 0;
            } else {
                pauseCtx->nameColorSet = 1;
            }
        }

        if ((pauseCtx->cursorX[PAUSE_EQUIP] == 0) && (pauseCtx->cursorY[PAUSE_EQUIP] == 0)) {
            if (LINK_AGE_IN_YEARS != YEARS_CHILD) {
                if ((cursorItem >= ITEM_BULLET_BAG_30) && (cursorItem <= ITEM_BULLET_BAG_50)) {
                    pauseCtx->nameColorSet = 1;
                } else {
                    pauseCtx->nameColorSet = 0;
                }
            } else {
                pauseCtx->nameColorSet = 0;
            }
        }

        KaleidoScope_SetCursorVtx(pauseCtx, cursorSlot * 4, pauseCtx->equipVtx);

        // Allow Toggling of Strength when Pressing A on Strength Upgrade Slot
        if ((pauseCtx->cursorSpecialPos == 0) && (pauseCtx->state == 6) && (pauseCtx->unk_1E4 == 0) &&
            CHECK_BTN_ALL(input->press.button, BTN_A) && (pauseCtx->cursorX[PAUSE_EQUIP] == 0) &&
            (pauseCtx->cursorY[PAUSE_EQUIP] == 2) && CVarGetInteger(CVAR_ENHANCEMENT("ToggleStrength"), 0)) {
            CVarSetInteger(CVAR_ENHANCEMENT("StrengthDisabled"),
                           !CVarGetInteger(CVAR_ENHANCEMENT("StrengthDisabled"), 0));
            // Equip success sound
            Audio_PlaySfxGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            // Wait 10 frames before accepting input again
            pauseCtx->unk_1E4 = 7;
            sEquipTimer = 10;
        }

        u16 buttonsToCheck = BTN_A | BTN_CLEFT | BTN_CDOWN | BTN_CRIGHT;
        if (CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0) &&
            (!CVarGetInteger(CVAR_SETTING("DPadOnPause"), 0) || CHECK_BTN_ALL(input->cur.button, BTN_CUP))) {
            buttonsToCheck |= BTN_DUP | BTN_DDOWN | BTN_DLEFT | BTN_DRIGHT;
        }

        if ((pauseCtx->cursorSpecialPos == 0) && (cursorItem != PAUSE_ITEM_NONE) && (pauseCtx->state == 6) &&
            (pauseCtx->unk_1E4 == 0) && CHECK_BTN_ANY(input->press.button, buttonsToCheck) &&
            (pauseCtx->cursorX[PAUSE_EQUIP] != 0)) {

            if (CHECK_AGE_REQ_EQUIP(pauseCtx->cursorY[PAUSE_EQUIP], pauseCtx->cursorX[PAUSE_EQUIP])) {
                if (CHECK_BTN_ALL(input->press.button, BTN_A)) {

                    // #Region SoH [Enhancements]
                    // Allow Link to remove his equipment from the equipment subscreen by toggling on/off
                    // Shields will be un-equipped entirely, and tunics/boots will revert to Kokiri Tunic/Kokiri Boots
                    // Only BGS/Giant's Knife is affected, and it will revert to Master Sword.

                    // If we have the feature toggled on
                    if (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentCanBeRemoved"), 0)) {

                        if (CVarGetInteger(CVAR_ENHANCEMENT("SwordToggle"), SWORD_TOGGLE_NONE) ==
                                SWORD_TOGGLE_BOTH_AGES ||
                            (CVarGetInteger(CVAR_ENHANCEMENT("SwordToggle"), SWORD_TOGGLE_NONE) ==
                             SWORD_TOGGLE_CHILD) &&
                                LINK_IS_CHILD) {
                            // If we're on the "swords" section of the equipment screen AND we're on a
                            // currently-equipped sword
                            if (pauseCtx->cursorY[PAUSE_EQUIP] == 0 &&
                                pauseCtx->cursorX[PAUSE_EQUIP] == CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD)) {
                                Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_NONE);
                                gSaveContext.equips.buttonItems[0] = ITEM_NONE;
                                Flags_SetInfTable(INFTABLE_SWORDLESS);
                                goto RESUME_EQUIPMENT_SWORD; // Skip to here so we don't re-equip it
                            }
                        } else {
                            // If we're on the "swords" section of the equipment screen AND we're on a
                            // currently-equipped BGS/Giant's Knife
                            if (pauseCtx->cursorY[PAUSE_EQUIP] == 0 && pauseCtx->cursorX[PAUSE_EQUIP] == 3 &&
                                CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_BIGGORON &&
                                CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD,
                                                  EQUIP_INV_SWORD_MASTER)) { // And we have the Master Sword
                                Inventory_ChangeEquipment(
                                    EQUIP_TYPE_SWORD,
                                    EQUIP_VALUE_SWORD_MASTER); // "Unequip" it by equipping Master Sword
                                gSaveContext.equips.buttonItems[0] = ITEM_SWORD_MASTER;
                                Flags_UnsetInfTable(INFTABLE_SWORDLESS);
                                goto RESUME_EQUIPMENT_SWORD; // Skip to here so we don't re-equip it
                            }
                        }

                        // If we're on the "shields" section of the equipment screen AND we're on a currently-equipped
                        // shield
                        if (pauseCtx->cursorY[PAUSE_EQUIP] == 1 &&
                            pauseCtx->cursorX[PAUSE_EQUIP] == CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD)) {
                            Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_NONE); // Unequip it
                            goto RESUME_EQUIPMENT; // Skip to here so we don't re-equip it
                        }

                        // If we're on the "tunics" section of the equipment screen AND we're on a currently-equipped
                        // tunic
                        if (pauseCtx->cursorY[PAUSE_EQUIP] == 2 &&
                            pauseCtx->cursorX[PAUSE_EQUIP] == CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC)) {
                            Inventory_ChangeEquipment(
                                EQUIP_TYPE_TUNIC, EQUIP_VALUE_TUNIC_KOKIRI); // "Unequip" it (by equipping Kokiri Tunic)
                            goto RESUME_EQUIPMENT;                           // Skip to here so we don't re-equip it
                        }

                        // If we're on the "boots" section of the equipment screen AND we're on currently-equipped boots
                        if (pauseCtx->cursorY[PAUSE_EQUIP] == 3 &&
                            pauseCtx->cursorX[PAUSE_EQUIP] == CUR_EQUIP_VALUE(EQUIP_TYPE_BOOTS)) {
                            Inventory_ChangeEquipment(
                                EQUIP_TYPE_BOOTS, EQUIP_VALUE_BOOTS_KOKIRI); // "Unequip" it (by equipping Kokiri Boots)
                            goto RESUME_EQUIPMENT;                           // Skip to here so we don't re-equip it
                        }
                    }

                    if (CHECK_OWNED_EQUIP(pauseCtx->cursorY[PAUSE_EQUIP], pauseCtx->cursorX[PAUSE_EQUIP] - 1)) {
                        Inventory_ChangeEquipment(pauseCtx->cursorY[PAUSE_EQUIP], pauseCtx->cursorX[PAUSE_EQUIP]);
                    } else {
                        goto EQUIP_FAIL;
                    }

                RESUME_EQUIPMENT:
                    if (pauseCtx->cursorY[PAUSE_EQUIP] == 0) {
                        gSaveContext.infTable[29] = 0;
                        gSaveContext.equips.buttonItems[0] = cursorItem;

                        if ((pauseCtx->cursorX[PAUSE_EQUIP] == 3) && (gSaveContext.bgsFlag != 0)) {
                            gSaveContext.equips.buttonItems[0] = ITEM_SWORD_BGS;
                            gSaveContext.swordHealth = 8;
                        } else {
                            if (gSaveContext.equips.buttonItems[0] == ITEM_HEART_PIECE_2) {
                                gSaveContext.equips.buttonItems[0] = ITEM_SWORD_BGS;
                            }
                            if ((gSaveContext.equips.buttonItems[0] == ITEM_SWORD_BGS) && (gSaveContext.bgsFlag == 0) &&
                                CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE)) {
                                gSaveContext.equips.buttonItems[0] = ITEM_SWORD_KNIFE;
                            }
                        }
                    RESUME_EQUIPMENT_SWORD:
                        Interface_LoadItemIcon1(play, 0);
                    }

                    Audio_PlaySfxGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                    pauseCtx->unk_1E4 = 7;
                    sEquipTimer = 10;
                } else if (CVarGetInteger(CVAR_ENHANCEMENT("AssignableTunicsAndBoots"), 0) != 0) {
                    // Only allow assigning shield, tunic and boots to c-buttons
                    if (pauseCtx->cursorY[PAUSE_EQUIP] > 0) {
                        if (CHECK_OWNED_EQUIP(pauseCtx->cursorY[PAUSE_EQUIP], pauseCtx->cursorX[PAUSE_EQUIP] - 1)) {
                            u16 slot = 0;
                            switch (cursorItem) {
                                case ITEM_TUNIC_KOKIRI:
                                    slot = SLOT_TUNIC_KOKIRI;
                                    break;
                                case ITEM_TUNIC_GORON:
                                    slot = SLOT_TUNIC_GORON;
                                    break;
                                case ITEM_TUNIC_ZORA:
                                    slot = SLOT_TUNIC_ZORA;
                                    break;
                                case ITEM_BOOTS_KOKIRI:
                                    slot = SLOT_BOOTS_KOKIRI;
                                    break;
                                case ITEM_BOOTS_IRON:
                                    slot = SLOT_BOOTS_IRON;
                                    break;
                                case ITEM_BOOTS_HOVER:
                                    slot = SLOT_BOOTS_HOVER;
                                    break;
                                case ITEM_SHIELD_DEKU:
                                    slot = SLOT_SHIELD_DEKU;
                                    break;
                                case ITEM_SHIELD_HYLIAN:
                                    slot = SLOT_SHIELD_HYLIAN;
                                    break;
                                case ITEM_SHIELD_MIRROR:
                                    slot = SLOT_SHIELD_MIRROR;
                                    break;
                                default:
                                    break;
                            }
                            if (GameInteractor_Should(VB_EQUIP_ITEM_TO_C_BUTTON, true, play, slot, cursorItem)) {
                                KaleidoScope_SetupItemEquip(play, cursorItem, slot,
                                                            pauseCtx->equipVtx[cursorSlot * 4].v.ob[0] * 10,
                                                            pauseCtx->equipVtx[cursorSlot * 4].v.ob[1] * 10);
                            }
                        } else {
                            Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                        }
                    }
                }
            } else {
            EQUIP_FAIL:
                if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
                    Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                } else if ((CVarGetInteger(CVAR_ENHANCEMENT("AssignableTunicsAndBoots"), 0) != 0) &&
                           (pauseCtx->cursorY[PAUSE_EQUIP] > 1)) {
                    Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                }
            }
        }

        if (oldCursorPoint != pauseCtx->cursorPoint[PAUSE_EQUIP]) {
            Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
    } else if ((pauseCtx->unk_1E4 == 7) && (pauseCtx->pageIndex == PAUSE_EQUIP)) {
        KaleidoScope_SetCursorVtx(pauseCtx, pauseCtx->cursorSlot[PAUSE_EQUIP] * 4, pauseCtx->equipVtx);
        pauseCtx->cursorColorSet = 8;

        sEquipTimer--;
        if (sEquipTimer == 0) {
            pauseCtx->unk_1E4 = 0;
        }
    }

    // Grey Out Strength Upgrade Name when Disabled
    // Do not Grey Out Strength Upgrade Name when Enabled
    // This needs to be outside the previous block since otherwise the nameColorSet is cleared to 0 by other menu pages
    // when toggling
    if ((pauseCtx->pageIndex == PAUSE_EQUIP) && (pauseCtx->cursorX[PAUSE_EQUIP] == 0) &&
        (pauseCtx->cursorY[PAUSE_EQUIP] == 2) && CVarGetInteger(CVAR_ENHANCEMENT("ToggleStrength"), 0)) {
        if (CVarGetInteger(CVAR_ENHANCEMENT("StrengthDisabled"), 0)) {
            pauseCtx->nameColorSet = 1;
        } else {
            pauseCtx->nameColorSet = 0;
        }
    }

    for (rowStart = 0, i = 0, point = 4; i < 4; i++, rowStart += 4, point += 16) {

        for (k = 0, temp = rowStart + 1, bit = rowStart, j = point; k < 3; k++, bit++, j += 4, temp++) {

            if ((gBitFlags[bit] & gSaveContext.inventory.equipment) && (pauseCtx->cursorSpecialPos == 0)) {
                if (CHECK_AGE_REQ_EQUIP(i, k + 1)) {
                    if (temp == cursorSlot) {
                        pauseCtx->equipVtx[j].v.ob[0] = pauseCtx->equipVtx[j + 2].v.ob[0] =
                            pauseCtx->equipVtx[j].v.ob[0] - 2;
                        pauseCtx->equipVtx[j + 1].v.ob[0] = pauseCtx->equipVtx[j + 3].v.ob[0] =
                            pauseCtx->equipVtx[j + 1].v.ob[0] + 4;
                        pauseCtx->equipVtx[j].v.ob[1] = pauseCtx->equipVtx[j + 1].v.ob[1] =
                            pauseCtx->equipVtx[j].v.ob[1] + 2;
                        pauseCtx->equipVtx[j + 2].v.ob[1] = pauseCtx->equipVtx[j + 3].v.ob[1] =
                            pauseCtx->equipVtx[j + 2].v.ob[1] - 4;
                        if (tabbedLayout) {
                            // Crescimento simetrico (+2 por lado) no layout em abas
                            pauseCtx->equipVtx[j + 1].v.ob[0] = pauseCtx->equipVtx[j + 3].v.ob[0] =
                                pauseCtx->equipVtx[j + 1].v.ob[0] - 2;
                            pauseCtx->equipVtx[j + 2].v.ob[1] = pauseCtx->equipVtx[j + 3].v.ob[1] =
                                pauseCtx->equipVtx[j + 2].v.ob[1] + 2;
                        }
                    }
                }
            }
        }
    }

    // Add zoom effect to strength item if cursor is hovering over it when toggle option is on
    if ((pauseCtx->cursorX[PAUSE_EQUIP] == 0) && (pauseCtx->cursorY[PAUSE_EQUIP] == 2) &&
        CVarGetInteger(CVAR_ENHANCEMENT("ToggleStrength"), 0) && pauseCtx->cursorSpecialPos == 0) {
        u8 row = 2;
        u8 column = 0;
        u8 equipVtxIndex = 16 * row + 4 * column;
        pauseCtx->equipVtx[equipVtxIndex].v.ob[0] = pauseCtx->equipVtx[equipVtxIndex + 2].v.ob[0] =
            pauseCtx->equipVtx[equipVtxIndex].v.ob[0] - 2;
        pauseCtx->equipVtx[equipVtxIndex + 1].v.ob[0] = pauseCtx->equipVtx[equipVtxIndex + 3].v.ob[0] =
            pauseCtx->equipVtx[equipVtxIndex + 1].v.ob[0] + 4;
        pauseCtx->equipVtx[equipVtxIndex].v.ob[1] = pauseCtx->equipVtx[equipVtxIndex + 1].v.ob[1] =
            pauseCtx->equipVtx[equipVtxIndex].v.ob[1] + 2;
        pauseCtx->equipVtx[equipVtxIndex + 2].v.ob[1] = pauseCtx->equipVtx[equipVtxIndex + 3].v.ob[1] =
            pauseCtx->equipVtx[equipVtxIndex + 2].v.ob[1] - 4;
    }

    if (tabbedLayout) {
        KaleidoScope_DrawEquipFrames(play);
    }

    Gfx_SetupDL_42Opa(play->state.gfxCtx);

    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);

    for (rowStart = 0, j = 0, temp = 0, i = 0; i < 4; i++, rowStart += 4, j += 16) {
        gSPVertex(POLY_OPA_DISP++, &pauseCtx->equipVtx[j], 16, 0);
        if (LINK_AGE_IN_YEARS == YEARS_CHILD) {
            point = CUR_UPG_VALUE(sChildUpgrades[i]);
            if ((point != 0) && (CUR_UPG_VALUE(sChildUpgrades[i]) != 0)) {
                int upgradeItem = sChildUpgradeItemBases[i] + point - 1;
                // Grey Out the Gauntlets as Child
                // Grey Out Strength Upgrades when Disabled and the Toggle Strength Option is on
                if (!CHECK_AGE_REQ_ITEM(upgradeItem) ||
                    (CVarGetInteger(CVAR_ENHANCEMENT("ToggleStrength"), 0) &&
                     CVarGetInteger(CVAR_ENHANCEMENT("StrengthDisabled"), 0) && sChildUpgrades[i] == UPG_STRENGTH)) {
                    gDPSetGrayscaleColor(POLY_OPA_DISP++, 109, 109, 109, 255);
                    gSPGrayscale(POLY_OPA_DISP++, true);
                }
                KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, gItemIcons[upgradeItem], 32, 32, 0);
                gSPGrayscale(POLY_OPA_DISP++, false);
            }
        } else {
            if ((i == 0) &&
                (CUR_UPG_VALUE(sAdultUpgrades[i]) ==
                 0)) { // If the player doesn't have the bow, load the current slingshot ammo upgrade instead.
                // Check the base item, the upgrade level can be 0 here
                if (!CHECK_AGE_REQ_ITEM(sChildUpgradeItemBases[i])) {
                    gDPSetGrayscaleColor(POLY_OPA_DISP++, 109, 109, 109, 255); // Grey Out Slingshot Bullet Bags
                    gSPGrayscale(POLY_OPA_DISP++, true);
                }
                KaleidoScope_DrawQuadTextureRGBA32(
                    play->state.gfxCtx, gItemIcons[sChildUpgradeItemBases[i] + CUR_UPG_VALUE(sChildUpgrades[i]) - 1],
                    32, 32, 0);
                gSPGrayscale(POLY_OPA_DISP++, false);
            } else if (CUR_UPG_VALUE(sAdultUpgrades[i]) != 0) {
                int upgradeItem = sAdultUpgradeItemBases[i] + CUR_UPG_VALUE(sAdultUpgrades[i]) - 1;
                // Grey Out upgrades the wrong age, ie the Goron Bracelet as Adult,
                // unless rando or the Toggle Strength Option still has a use for it
                // Grey Out Strength Upgrades when Disabled and the Toggle Strength Option is on
                if ((!CHECK_AGE_REQ_ITEM(upgradeItem) && !(IS_RANDO) &&
                     !CVarGetInteger(CVAR_ENHANCEMENT("ToggleStrength"), 0)) ||
                    (CVarGetInteger(CVAR_ENHANCEMENT("ToggleStrength"), 0) &&
                     CVarGetInteger(CVAR_ENHANCEMENT("StrengthDisabled"), 0) && sAdultUpgrades[i] == UPG_STRENGTH)) {
                    gDPSetGrayscaleColor(POLY_OPA_DISP++, 109, 109, 109, 255);
                    gSPGrayscale(POLY_OPA_DISP++, true);
                }
                KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, gItemIcons[upgradeItem], 32, 32, 0);
                gSPGrayscale(POLY_OPA_DISP++, false);
            }
        }
        // Draw inventory screen icons
        for (k = 0, bit = rowStart, point = 4; k < 3; k++, point += 4, temp++, bit++) {

            int itemId = ITEM_SWORD_KOKIRI + temp;
            bool age_restricted = !CHECK_AGE_REQ_ITEM(itemId);
            if (age_restricted) {
                gDPSetGrayscaleColor(POLY_OPA_DISP++, 109, 109, 109, 255);
                gSPGrayscale(POLY_OPA_DISP++, true);
            }
            if (((u32)i == 0) && (k == 2) && (gSaveContext.bgsFlag != 0)) {
                KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, gItemIconSwordBiggoronTex, 32, 32, point);
            } else if ((i == 0) && (k == 2) && (gBitFlags[bit + 1] & gSaveContext.inventory.equipment)) {
                KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, gItemIconBrokenGiantsKnifeTex, 32, 32, point);
            } else if (gBitFlags[bit] & gSaveContext.inventory.equipment) {
                KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, gItemIcons[itemId], 32, 32, point);
            }
            gSPGrayscale(POLY_OPA_DISP++, false);
        }
    }

    // Render A button indicator when hovered over strength
    if ((pauseCtx->cursorX[PAUSE_EQUIP] == 0) && (pauseCtx->cursorY[PAUSE_EQUIP] == 2) &&
        CVarGetInteger(CVAR_ENHANCEMENT("ToggleStrength"), 0) && pauseCtx->cursorSpecialPos == 0 &&
        pauseCtx->unk_1E4 == 0 && pauseCtx->state == 6) {
        u8 row = 2;
        u8 column = 0;
        u8 equipVtxIndex = 16 * row + 4 * column;
        // Get Bottom Bisector of the Quad
        s16 translateX =
            (pauseCtx->equipVtx[equipVtxIndex].v.ob[0] + pauseCtx->equipVtx[equipVtxIndex + 1].v.ob[0]) / 2;
        // Add 4 since the icon will be zoomed in on
        s16 translateY = pauseCtx->equipVtx[equipVtxIndex + 2].v.ob[1] + 4;
        KaleidoScope_DrawAButton(play, sStrengthAButtonVtx, translateX, translateY);
    }

    KaleidoScope_DrawPlayerWork(play);

    // if ((pauseCtx->unk_1E4 == 7) && (sEquipTimer == 10)) {
    // KaleidoScope_SetupPlayerPreRender(play);
    //}

    if ((pauseCtx->unk_1E4 == 7) && (sEquipTimer == 9)) {
        //! @bug: This function shouldn't take any arguments
        // KaleidoScope_ProcessPlayerPreRender(play);
    }

    // gSPInvalidateTexCache(POLY_OPA_DISP++, 0);
    gSPInvalidateTexCache(POLY_OPA_DISP++, pauseCtx->iconItemSegment);
    // gSPInvalidateTexCache(POLY_OPA_DISP++, pauseCtx->iconItem24Segment);
    gSPInvalidateTexCache(POLY_OPA_DISP++, pauseCtx->nameSegment);

    // gSPSegment(POLY_OPA_DISP++, 0x07, pauseCtx->playerSegment);
    gSPSegment(POLY_OPA_DISP++, 0x08, pauseCtx->iconItemSegment);
    gSPSegment(POLY_OPA_DISP++, 0x09, pauseCtx->iconItem24Segment);
    gSPSegment(POLY_OPA_DISP++, 0x0A, pauseCtx->nameSegment);
    gSPSegment(POLY_OPA_DISP++, 0x0B, play->interfaceCtx.mapSegment);
    // gSPSegment(POLY_OPA_DISP++, 0x0C, pauseCtx->iconItemAltSegment);

    Gfx_SetupDL_42Opa(play->state.gfxCtx);
    if (tabbedLayout) {
        KaleidoScope_DrawEquipOvalImage(play);
    } else {
        KaleidoScope_DrawEquipmentImage(play, pauseCtx->playerSegment, PAUSE_EQUIP_PLAYER_WIDTH,
                                        PAUSE_EQUIP_PLAYER_HEIGHT);
    }

    if (gUpgradeMasks[0]) {}

    CLOSE_DISPS(play->state.gfxCtx);
}
