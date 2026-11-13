#include <libultraship/bridge/consolevariablebridge.h>
#include "libultraship/libultra/controller.h"
#include <ship/Context.h>
#include <libultraship/controller/controldeck/ControlDeck.h>
#include <fast/Fast3dGui.h>
#include <imgui.h>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include "OcarinaSongMenu.h"
#include "soh/OTRGlobals.h"
#include "textures/parameter_static/parameter_static.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
#include "functions.h"
extern OcarinaNote sOcarinaSongNotes[OCARINA_SONG_MAX][20];
extern PlayState* gPlayState;
// Definida em code_800EC960.c mas não declarada nos headers (marcada como "Unused" no jogo)
void AudioOcarina_EnableInput(u8 inputEnabled);
// Acessores adicionados em code_800EC960.c
u8 AudioOcarina_IsActive(void);
u8 AudioOcarina_IsInputEnabled(void);
}

// TEMPORÁRIO: mostra uma linha de diagnóstico no canto da tela. Mude para 0 depois de resolver.
#define OSM_DEBUG 0

namespace {

#define CVAR_OCA_MENU_NAME CVAR_ENHANCEMENT("OcarinaSongMenu")

// Botões de controle usados pelo menu (mude aqui se quiser outro mapeamento)
constexpr int kBtnToggle = BTN_DDOWN; // abre/fecha o menu
constexpr int kBtnPrev = BTN_DLEFT;
constexpr int kBtnNext = BTN_DRIGHT;
constexpr int kBtnListen = BTN_Z; // ouvir a melodia

constexpr float kPi = 3.14159265f;

// Cor do disco escuro de cada bolinha (também usada para "recortar" formas, como a lua)
const ImU32 kDiscColor = IM_COL32(25, 25, 30, 235);

struct SongInfo {
    const char* name;
    const char* desc;
    int songId;    // OCARINA_SONG_*
    int questItem; // QUEST_SONG_*
    ImU32 color;   // cor principal do ícone
};

const SongInfo kSongs[] = {
    { "Zelda's Lullaby", "A gentle melody for the royal family.", OCARINA_SONG_LULLABY, QUEST_SONG_LULLABY,
      IM_COL32(230, 200, 90, 255) },
    { "Epona's Song", "A melody that calls your horse.", OCARINA_SONG_EPONAS, QUEST_SONG_EPONA,
      IM_COL32(210, 140, 80, 255) },
    { "Saria's Song", "A cheerful tune from a friend in the forest.", OCARINA_SONG_SARIAS, QUEST_SONG_SARIA,
      IM_COL32(90, 200, 100, 255) },
    { "Sun's Song", "Turns day to night, and night to day.", OCARINA_SONG_SUNS, QUEST_SONG_SUN,
      IM_COL32(250, 200, 60, 255) },
    { "Song of Time", "Opens the Door of Time.", OCARINA_SONG_TIME, QUEST_SONG_TIME, IM_COL32(120, 160, 240, 255) },
    { "Song of Storms", "Calls forth rain and storms.", OCARINA_SONG_STORMS, QUEST_SONG_STORMS,
      IM_COL32(170, 175, 215, 255) },
    { "Minuet of Forest", "This melody will take you to the Sacred Forest Meadow.", OCARINA_SONG_MINUET,
      QUEST_SONG_MINUET, IM_COL32(70, 190, 90, 255) },
    { "Bolero of Fire", "This melody will take you to Death Mountain Crater.", OCARINA_SONG_BOLERO, QUEST_SONG_BOLERO,
      IM_COL32(240, 80, 50, 255) },
    { "Serenade of Water", "This whimsical melody will take you to Lake Hylia.", OCARINA_SONG_SERENADE,
      QUEST_SONG_SERENADE, IM_COL32(80, 130, 240, 255) },
    { "Requiem of Spirit", "This melody will take you to the Desert Colossus.", OCARINA_SONG_REQUIEM,
      QUEST_SONG_REQUIEM, IM_COL32(235, 160, 70, 255) },
    { "Nocturne of Shadow", "This melody will take you to the Graveyard.", OCARINA_SONG_NOCTURNE, QUEST_SONG_NOCTURNE,
      IM_COL32(170, 80, 190, 255) },
    { "Prelude of Light", "This melody will take you to the Temple of Time.", OCARINA_SONG_PRELUDE, QUEST_SONG_PRELUDE,
      IM_COL32(245, 235, 140, 255) },
};
constexpr int kSongCount = (int)(sizeof(kSongs) / sizeof(kSongs[0]));

enum BtnNote { BN_A = 0, BN_DOWN, BN_RIGHT, BN_LEFT, BN_UP, BN_NONE };

BtnNote PitchToButton(u8 pitch) {
    switch (pitch) {
        case OCARINA_PITCH_D4:
            return BN_A;
        case OCARINA_PITCH_F4:
            return BN_DOWN;
        case OCARINA_PITCH_A4:
            return BN_RIGHT;
        case OCARINA_PITCH_B4:
            return BN_LEFT;
        case OCARINA_PITCH_D5:
            return BN_UP;
        default:
            return BN_NONE;
    }
}

// Lê as notas direto da tabela do jogo
std::vector<BtnNote> GetSongButtons(int songId) {
    std::vector<BtnNote> out;
    for (int i = 0; i < 20; i++) {
        u8 pitch = sOcarinaSongNotes[songId][i].pitch;
        if (pitch == OCARINA_PITCH_NONE) {
            if (sOcarinaSongNotes[songId][i].length == 0) {
                break; // fim da música
            }
            continue;
        }
        BtnNote b = PitchToButton(pitch);
        if (b != BN_NONE) {
            out.push_back(b);
        }
    }
    return out;
}

// ---- Estado do menu ----
bool sOpen = false;
bool sSavedInputEnabled = false;
bool sMelodyStarted = false;
int sSel = 0;
int sPrevButtons = 0;
int sPrevStickDir = 0;

bool IsSongLearned(const SongInfo& s) {
    return CHECK_QUEST_ITEM(s.questItem);
}

bool IsGameplayReady() {
    return gPlayState != nullptr && gSaveContext.gameMode == 0 && gPlayState->pauseCtx.state == 0;
}

void OpenMenu() {
    sOpen = true;
    sSavedInputEnabled = AudioOcarina_IsInputEnabled() != 0;
    AudioOcarina_EnableInput(false); // impede A/C de tocar notas enquanto o menu está aberto
}

void CloseMenu() {
    if (!sOpen) {
        return;
    }
    sOpen = false;
    if (sMelodyStarted) {
        AudioOcarina_SetPlaybackSong(0, 0);
        sMelodyStarted = false;
    }
    if (AudioOcarina_IsActive()) {
        AudioOcarina_EnableInput(sSavedInputEnabled);
    }
}

void PlaySelectedMelody() {
    // O playback recebe "índice da música + 1" (0 = parar)
    AudioOcarina_SetPlaybackSong((s8)(kSongs[sSel].songId + 1), 1);
    sMelodyStarted = true;
}

// ---------------------------------------------------------------------------
// Ícones desenhados por código
// ---------------------------------------------------------------------------

// Forma de "lente" (folha, olho). Convexa. angle em radianos.
void DrawLens(ImDrawList* dl, ImVec2 c, float halfLen, float halfWid, float angle, ImU32 col) {
    const int n = 10;
    ImVec2 pts[2 * n];
    int count = 0;
    const float ca = cosf(angle), sa = sinf(angle);
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)(n - 1);
        float u = (t - 0.5f) * 2.0f * halfLen;
        float v = -sinf(kPi * t) * halfWid;
        pts[count++] = ImVec2(c.x + u * ca - v * sa, c.y + u * sa + v * ca);
    }
    for (int i = n - 2; i >= 1; i--) {
        float t = (float)i / (float)(n - 1);
        float u = (t - 0.5f) * 2.0f * halfLen;
        float v = sinf(kPi * t) * halfWid;
        pts[count++] = ImVec2(c.x + u * ca - v * sa, c.y + u * sa + v * ca);
    }
    dl->AddConvexPolyFilled(pts, count, col);
}

// Lua crescente: círculo menos um círculo deslocado (recorte na cor do disco)
void DrawCrescent(ImDrawList* dl, ImVec2 c, float rad, ImU32 col) {
    dl->AddCircleFilled(c, rad, col, 32);
    dl->AddCircleFilled(ImVec2(c.x + rad * 0.45f, c.y - rad * 0.3f), rad * 0.85f, kDiscColor, 32);
}

// Desenha o ícone da música "idx" centrado em c, cabendo em um círculo de raio rad
void DrawSongIcon(ImDrawList* dl, ImVec2 c, float rad, int idx, ImU32 col) {
    const float R = rad;
    switch (idx) {
        case 0: { // Zelda's Lullaby: Triforce
            const float s = R * 0.95f;
            ImVec2 apex(c.x, c.y - s * 0.9f);
            ImVec2 bl(c.x - s, c.y + s * 0.8f);
            ImVec2 br(c.x + s, c.y + s * 0.8f);
            ImVec2 bm(c.x, c.y + s * 0.8f);
            ImVec2 ml(c.x - s * 0.5f, c.y - s * 0.05f);
            ImVec2 mr(c.x + s * 0.5f, c.y - s * 0.05f);
            dl->AddTriangleFilled(apex, ml, mr, col);
            dl->AddTriangleFilled(ml, bl, bm, col);
            dl->AddTriangleFilled(mr, bm, br, col);
            break;
        }
        case 1: { // Epona's Song: ferradura
            dl->PathArcTo(c, R * 0.62f, kPi * 0.62f, kPi * 2.38f, 24);
            dl->PathStroke(col, 0, R * 0.3f);
            for (int k = 0; k < 3; k++) {
                float a = kPi * (1.0f + 0.5f * (float)k);
                dl->AddCircleFilled(ImVec2(c.x + cosf(a) * R * 0.62f, c.y + sinf(a) * R * 0.62f), R * 0.07f,
                                    kDiscColor);
            }
            break;
        }
        case 2: { // Saria's Song: folha
            const float a = -kPi * 0.25f;
            DrawLens(dl, c, R * 0.95f, R * 0.55f, a, col);
            ImVec2 d(cosf(a) * R * 0.75f, sinf(a) * R * 0.75f);
            dl->AddLine(ImVec2(c.x - d.x, c.y - d.y), ImVec2(c.x + d.x, c.y + d.y), kDiscColor, 1.5f);
            break;
        }
        case 3: { // Sun's Song: sol e lua
            ImVec2 sc(c.x - R * 0.2f, c.y - R * 0.15f);
            const float sr = R * 0.4f;
            dl->AddCircleFilled(sc, sr, col, 24);
            for (int k = 0; k < 8; k++) {
                float a = kPi * 0.25f * (float)k;
                dl->AddLine(ImVec2(sc.x + cosf(a) * sr * 1.35f, sc.y + sinf(a) * sr * 1.35f),
                            ImVec2(sc.x + cosf(a) * sr * 1.9f, sc.y + sinf(a) * sr * 1.9f), col, 2.5f);
            }
            DrawCrescent(dl, ImVec2(c.x + R * 0.5f, c.y + R * 0.45f), R * 0.35f, IM_COL32(235, 235, 255, 255));
            break;
        }
        case 4: { // Song of Time: ampulheta
            const float w = R * 0.6f, h = R * 0.85f;
            dl->AddTriangleFilled(ImVec2(c.x - w, c.y - h), ImVec2(c.x + w, c.y - h), ImVec2(c.x, c.y), col);
            dl->AddTriangleFilled(ImVec2(c.x - w, c.y + h), ImVec2(c.x + w, c.y + h), ImVec2(c.x, c.y), col);
            const float bar = R * 0.14f;
            dl->AddRectFilled(ImVec2(c.x - w - bar, c.y - h - bar), ImVec2(c.x + w + bar, c.y - h + bar),
                              IM_COL32(235, 235, 255, 255), 2.0f);
            dl->AddRectFilled(ImVec2(c.x - w - bar, c.y + h - bar), ImVec2(c.x + w + bar, c.y + h + bar),
                              IM_COL32(235, 235, 255, 255), 2.0f);
            break;
        }
        case 5: { // Song of Storms: nuvem e raio
            dl->AddCircleFilled(ImVec2(c.x - R * 0.4f, c.y - R * 0.15f), R * 0.35f, col, 20);
            dl->AddCircleFilled(ImVec2(c.x + R * 0.05f, c.y - R * 0.35f), R * 0.42f, col, 20);
            dl->AddCircleFilled(ImVec2(c.x + R * 0.45f, c.y - R * 0.1f), R * 0.32f, col, 20);
            dl->AddRectFilled(ImVec2(c.x - R * 0.5f, c.y - R * 0.15f), ImVec2(c.x + R * 0.55f, c.y + R * 0.17f), col,
                              4.0f);
            ImVec2 p0(c.x + R * 0.1f, c.y + R * 0.0f);
            ImVec2 p1(c.x - R * 0.2f, c.y + R * 0.45f);
            ImVec2 p2(c.x + R * 0.05f, c.y + R * 0.45f);
            ImVec2 p3(c.x - R * 0.15f, c.y + R * 0.95f);
            const ImU32 bolt = IM_COL32(255, 230, 70, 255);
            dl->AddLine(p0, p1, bolt, 3.0f);
            dl->AddLine(p1, p2, bolt, 3.0f);
            dl->AddLine(p2, p3, bolt, 3.0f);
            break;
        }
        case 6: { // Minuet of Forest: árvore
            dl->AddRectFilled(ImVec2(c.x - R * 0.12f, c.y + R * 0.5f), ImVec2(c.x + R * 0.12f, c.y + R * 0.92f),
                              IM_COL32(140, 90, 50, 255));
            const float apexY[3] = { -0.2f, -0.55f, -0.95f };
            const float baseY[3] = { 0.6f, 0.15f, -0.25f };
            const float halfW[3] = { 0.8f, 0.62f, 0.45f };
            for (int k = 0; k < 3; k++) {
                ImVec2 apex(c.x, c.y + R * apexY[k]);
                ImVec2 l(c.x - R * halfW[k], c.y + R * baseY[k]);
                ImVec2 r(c.x + R * halfW[k], c.y + R * baseY[k]);
                dl->AddTriangleFilled(apex, l, r, col);
                dl->AddTriangle(apex, l, r, kDiscColor, 1.5f);
            }
            break;
        }
        case 7: { // Bolero of Fire: chama
            dl->AddCircleFilled(ImVec2(c.x, c.y + R * 0.3f), R * 0.55f, col, 24);
            dl->AddTriangleFilled(ImVec2(c.x + R * 0.05f, c.y - R * 0.95f), ImVec2(c.x - R * 0.5f, c.y + R * 0.15f),
                                  ImVec2(c.x + R * 0.5f, c.y + R * 0.15f), col);
            dl->AddTriangleFilled(ImVec2(c.x - R * 0.5f, c.y - R * 0.45f), ImVec2(c.x - R * 0.58f, c.y + R * 0.25f),
                                  ImVec2(c.x - R * 0.1f, c.y + R * 0.0f), col);
            const ImU32 inner = IM_COL32(255, 225, 110, 255);
            dl->AddCircleFilled(ImVec2(c.x, c.y + R * 0.4f), R * 0.28f, inner, 20);
            dl->AddTriangleFilled(ImVec2(c.x, c.y - R * 0.2f), ImVec2(c.x - R * 0.26f, c.y + R * 0.3f),
                                  ImVec2(c.x + R * 0.26f, c.y + R * 0.3f), inner);
            break;
        }
        case 8: { // Serenade of Water: gota
            dl->AddCircleFilled(ImVec2(c.x, c.y + R * 0.3f), R * 0.6f, col, 24);
            dl->AddTriangleFilled(ImVec2(c.x, c.y - R * 0.95f), ImVec2(c.x - R * 0.55f, c.y + R * 0.1f),
                                  ImVec2(c.x + R * 0.55f, c.y + R * 0.1f), col);
            dl->AddCircleFilled(ImVec2(c.x - R * 0.22f, c.y + R * 0.38f), R * 0.1f, IM_COL32(235, 245, 255, 255), 12);
            break;
        }
        case 9: { // Requiem of Spirit: olho
            DrawLens(dl, c, R * 0.95f, R * 0.55f, 0.0f, col);
            dl->AddCircleFilled(c, R * 0.33f, kDiscColor, 20);
            dl->AddCircleFilled(ImVec2(c.x - R * 0.1f, c.y - R * 0.1f), R * 0.1f, col, 12);
            break;
        }
        case 10: { // Nocturne of Shadow: lua crescente e estrela
            DrawCrescent(dl, c, R * 0.85f, col);
            dl->AddCircleFilled(ImVec2(c.x + R * 0.5f, c.y + R * 0.35f), R * 0.1f, col, 12);
            break;
        }
        case 11: { // Prelude of Light: estrela de luz
            for (int k = 0; k < 8; k++) {
                float a = kPi * 0.25f * (float)k;
                float len = (k % 2 == 0) ? R : R * 0.65f;
                float dx = cosf(a), dy = sinf(a);
                float px = -dy, py = dx;
                const float baseR = R * 0.2f, w = R * 0.17f;
                ImVec2 b1(c.x + dx * baseR + px * w, c.y + dy * baseR + py * w);
                ImVec2 b2(c.x + dx * baseR - px * w, c.y + dy * baseR - py * w);
                dl->AddTriangleFilled(ImVec2(c.x + dx * len, c.y + dy * len), b1, b2, col);
            }
            dl->AddCircleFilled(c, R * 0.3f, col, 20);
            break;
        }
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Texturas do jogo (parameter_static) registradas no ImGui uma única vez
// ---------------------------------------------------------------------------
// Mesma ordem do enum BtnNote: A, C-baixo, C-direita, C-esquerda, C-cima
const char* const kBtnTexNames[5] = { "OSM_BtnA", "OSM_BtnCDown", "OSM_BtnCRight", "OSM_BtnCLeft", "OSM_BtnCUp" };
const char* const kClefTexName = "OSM_TrebleClef";
bool sTexturesRegistered = false;

std::shared_ptr<Fast::Fast3dGui> GetFastGui() {
    return std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
}

void EnsureTexturesRegistered() {
    if (sTexturesRegistered) {
        return;
    }
    auto gui = GetFastGui();
    if (gui == nullptr) {
        return;
    }
    sTexturesRegistered = true;
    const char* paths[5] = { gOcarinaBtnIconATex, gOcarinaBtnIconCDownTex, gOcarinaBtnIconCRightTex,
                             gOcarinaBtnIconCLeftTex, gOcarinaBtnIconCUpTex };
    for (int i = 0; i < 5; i++) {
        gui->LoadGuiTexture(kBtnTexNames[i], paths[i], "", ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    }
    gui->LoadGuiTexture(kClefTexName, gOcarinaTrebleClefTex, "", ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
}

// Nota da pauta: botão do jogo (mesmas texturas da Quest Status) tingido como no jogo.
// Se não existir, tenta gSongNoteTex; se também não existir, desenha um bloquinho.
void DrawNoteBlock(ImDrawList* dl, ImVec2 c, BtnNote n) {
    // Cores padrão do jogo: A azul, botões C amarelos
    ImU32 tint = (n == BN_A) ? IM_COL32(80, 150, 255, 255) : IM_COL32(255, 255, 50, 255);

    auto gui = GetFastGui();
    if (gui != nullptr) {
        if ((int)n >= 0 && (int)n < 5) {
            auto btnTex = gui->GetTextureByName(kBtnTexNames[(int)n]);
            if (btnTex) {
                const float half = 17.0f;
                dl->AddImage(btnTex, ImVec2(c.x - half, c.y - half), ImVec2(c.x + half, c.y + half), ImVec2(0, 0),
                             ImVec2(1, 1), tint);
                return;
            }
        }
        auto tex = gui->GetTextureByName("gSongNoteTex");
        if (tex) {
            ImVec2 sz = gui->GetTextureSize("gSongNoteTex");
            const float h = 34.0f;
            float w = (sz.x > 0.0f && sz.y > 0.0f) ? h * sz.x / sz.y : h;
            dl->AddImage(tex, ImVec2(c.x - w * 0.5f, c.y - h * 0.5f), ImVec2(c.x + w * 0.5f, c.y + h * 0.5f),
                         ImVec2(0, 0), ImVec2(1, 1), tint);
            return;
        }
    }

    // Fallback: bloquinho com a letra/seta do botão
    const char* labels[] = { "A", "v", ">", "<", "^" };
    ImU32 fg = (n == BN_A) ? IM_COL32_WHITE : IM_COL32(40, 30, 10, 255);
    dl->AddRectFilled(ImVec2(c.x - 13, c.y - 11), ImVec2(c.x + 13, c.y + 11), tint, 4.0f);
    dl->AddRect(ImVec2(c.x - 13, c.y - 11), ImVec2(c.x + 13, c.y + 11), IM_COL32(40, 30, 20, 255), 4.0f, 0, 1.5f);
    ImVec2 ts = ImGui::CalcTextSize(labels[n]);
    dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), fg, labels[n]);
}

// Clave de sol no início da pauta (textura do jogo); sem textura, não desenha nada
void DrawClef(ImDrawList* dl, float left, float staffTop, float gap) {
    auto gui = GetFastGui();
    if (gui == nullptr) {
        return;
    }
    auto clef = gui->GetTextureByName(kClefTexName);
    if (!clef) {
        return;
    }
    ImVec2 sz = gui->GetTextureSize(kClefTexName);
    float ch = gap * 6.0f;
    float cw = (sz.x > 0.0f && sz.y > 0.0f) ? ch * sz.x / sz.y : ch * 0.5f;
    const float maxW = 56.0f;
    if (cw > maxW) {
        ch *= maxW / cw;
        cw = maxW;
    }
    float ccx = left + 32.0f;
    float ccy = staffTop + gap * 2.0f;
    dl->AddImage(clef, ImVec2(ccx - cw * 0.5f, ccy - ch * 0.5f), ImVec2(ccx + cw * 0.5f, ccy + ch * 0.5f),
                 ImVec2(0, 0), ImVec2(1, 1), IM_COL32(70, 48, 28, 255));
}

// Dica discreta no rodapé enquanto a Ocarina está em uso e o menu está fechado
void DrawOpenHint() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const char* text = "D-Pad Down: Song Menu";
    ImVec2 ts = ImGui::CalcTextSize(text);
    const float padX = 16.0f, padY = 8.0f;
    float cx = vp->Pos.x + vp->Size.x * 0.5f;
    float y = vp->Pos.y + vp->Size.y * 0.93f;
    ImVec2 pMin(cx - ts.x * 0.5f - padX, y - padY);
    ImVec2 pMax(cx + ts.x * 0.5f + padX, y + ts.y + padY);
    dl->AddRectFilled(pMin, pMax, IM_COL32(0, 0, 0, 150), 12.0f);
    dl->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 120), 12.0f, 0, 1.5f);
    dl->AddText(ImVec2(cx - ts.x * 0.5f, y), IM_COL32_WHITE, text);
}

void DrawMenu() {
    EnsureTexturesRegistered();
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    const float W = vp->Size.x, H = vp->Size.y;
    const float r = fminf(W, H) * 0.045f;
    const float cx = vp->Pos.x + W * 0.5f;

    // ---- Arco de bolinhas (duas fileiras, como no remake) ----
    const int topCount = (kSongCount + 1) / 2;
    const int botCount = kSongCount - topCount;
    for (int i = 0; i < kSongCount; i++) {
        bool top = i < topCount;
        int k = top ? i : i - topCount;
        int cnt = top ? topCount : botCount;
        float t = (cnt > 1) ? (float)k / (float)(cnt - 1) : 0.5f;
        float radX = W * (top ? 0.26f : 0.21f);
        float radY = H * (top ? 0.17f : 0.10f);
        ImVec2 c(cx + (t - 0.5f) * 2.0f * radX, vp->Pos.y + H * (top ? 0.30f : 0.42f) - sinf(3.14159f * t) * radY);

        bool learned = IsSongLearned(kSongs[i]);
        dl->AddCircleFilled(c, r, learned ? kDiscColor : IM_COL32(110, 110, 110, 150), 32);
        if (learned) {
            DrawSongIcon(dl, c, r * 0.72f, i, kSongs[i].color);
        }
        if (i == sSel) {
            dl->AddCircle(c, r + 4.0f, IM_COL32_WHITE, 0, 3.5f);
        }
        // mouse / toque
        if (ImGui::IsMouseClicked(0)) {
            float dx = io.MousePos.x - c.x, dy = io.MousePos.y - c.y;
            if (dx * dx + dy * dy <= r * r) {
                sSel = i;
            }
        }
    }

    // ---- Pergaminho ----
    const SongInfo& s = kSongs[sSel];
    bool learned = IsSongLearned(s);
    ImVec2 pMin(vp->Pos.x + W * 0.20f, vp->Pos.y + H * 0.58f);
    ImVec2 pMax(vp->Pos.x + W * 0.80f, vp->Pos.y + H * 0.86f);
    dl->AddRectFilled(pMin, pMax, IM_COL32(224, 202, 152, 250), 10.0f);
    dl->AddRect(pMin, pMax, IM_COL32(150, 108, 58, 255), 10.0f, 0, 4.0f);

    const char* title = learned ? s.name : "???";
    ImVec2 tsz = ImGui::CalcTextSize(title);
    dl->AddText(ImVec2((pMin.x + pMax.x - tsz.x) * 0.5f, pMin.y + 12), IM_COL32(70, 48, 28, 255), title);

    if (learned) {
        const float gap = (pMax.y - pMin.y) * 0.085f;
        const float left = pMin.x + (pMax.x - pMin.x) * 0.10f;
        const float right = pMax.x - (pMax.x - pMin.x) * 0.06f;
        const float staffTop = pMin.y + (pMax.y - pMin.y) * 0.30f;
        for (int l = 0; l < 5; l++) {
            dl->AddLine(ImVec2(left, staffTop + (float)l * gap), ImVec2(right, staffTop + (float)l * gap),
                        IM_COL32(70, 48, 28, 255), 1.5f);
        }
        DrawClef(dl, left, staffTop, gap);
        // Mais agudo = mais alto. Ordem de altura: A < v < > < < < ^
        std::vector<BtnNote> notes = GetSongButtons(s.songId);
        float step = fminf(46.0f, (right - left - 90.0f) / fmaxf(1.0f, (float)notes.size()));
        for (size_t i = 0; i < notes.size(); i++) {
            float x = left + 70.0f + (float)i * step;
            float y = staffTop + gap * (4.0f - (float)notes[i]);
            DrawNoteBlock(dl, ImVec2(x, y), notes[i]);
        }
        ImVec2 dsz = ImGui::CalcTextSize(s.desc);
        dl->AddText(ImVec2((pMin.x + pMax.x - dsz.x) * 0.5f, pMax.y - 34), IM_COL32(70, 48, 28, 255), s.desc);
    } else {
        const char* msg = "You haven't learned this song yet.";
        ImVec2 msz = ImGui::CalcTextSize(msg);
        dl->AddText(ImVec2((pMin.x + pMax.x - msz.x) * 0.5f, (pMin.y + pMax.y) * 0.5f), IM_COL32(70, 48, 28, 255),
                    msg);
    }

    // ---- Dicas de botão ----
    const char* hint = "D-Pad Left/Right: select      Z: listen to the melody      D-Pad Down: close";
    ImVec2 hsz = ImGui::CalcTextSize(hint);
    dl->AddText(ImVec2((pMin.x + pMax.x - hsz.x) * 0.5f, pMax.y + 10), IM_COL32_WHITE, hint);
}

} // namespace

// Sem moldura de janela: mesmo padrão do InputViewer
void OcarinaSongMenuWindow::Draw() {
    if (!IsVisible()) {
        return;
    }
    DrawElement();
    SyncVisibilityConsoleVariable();
}

void OcarinaSongMenuWindow::DrawElement() {
    // ---- Lê o controle e detecta "acabou de apertar" ----
    OSContPad* pads =
        std::dynamic_pointer_cast<LUS::ControlDeck>(Ship::Context::GetRawInstance()->GetControlDeck())->GetPads();
    int cur = 0;
    int stickDir = 0;
    if (pads != nullptr) {
        cur = pads[0].button;
        stickDir = (pads[0].stick_x > 40) - (pads[0].stick_x < -40);
    }
    int pressed = cur & ~sPrevButtons;
    int stickPressed = (stickDir != 0 && stickDir != sPrevStickDir) ? stickDir : 0;
    sPrevButtons = cur;
    sPrevStickDir = stickDir;

    // ---- Só funciona com a opção ligada, em gameplay e com a Ocarina ativa ----
    bool enabled = CVarGetInteger(CVAR_OCA_MENU_NAME, 1) != 0; // ligado por padrão
    bool ready = IsGameplayReady();
    bool active = AudioOcarina_IsActive() != 0;
    bool ocarinaActive = enabled && ready && active;

#if OSM_DEBUG
    {
        char dbg[256];
        snprintf(dbg, sizeof(dbg), "OSM debug | option=%d ready=%d ocarinaActive=%d pads=%s buttons=0x%04X pressed=0x%04X open=%d",
                 enabled ? 1 : 0, ready ? 1 : 0, active ? 1 : 0, pads != nullptr ? "ok" : "NULL", cur, pressed,
                 sOpen ? 1 : 0);
        ImVec2 pos(ImGui::GetMainViewport()->Pos.x + 10.0f, ImGui::GetMainViewport()->Pos.y + 40.0f);
        ImGui::GetForegroundDrawList()->AddText(ImVec2(pos.x + 1, pos.y + 1), IM_COL32(0, 0, 0, 255), dbg);
        ImGui::GetForegroundDrawList()->AddText(pos, IM_COL32(255, 255, 0, 255), dbg);
    }
#endif

    if (!ocarinaActive) {
        CloseMenu();
        return;
    }

    // ESC abre o menu do Ship of Harkinian: fecha o menu de músicas para não ficar por cima dele
    if (sOpen && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        CloseMenu();
        return;
    }

    if (pressed & kBtnToggle) {
        if (sOpen) {
            CloseMenu();
        } else {
            OpenMenu();
        }
    }
    if (!sOpen) {
        DrawOpenHint();
        return;
    }

    if ((pressed & kBtnNext) || stickPressed > 0) {
        sSel = (sSel + 1) % kSongCount;
    }
    if ((pressed & kBtnPrev) || stickPressed < 0) {
        sSel = (sSel + kSongCount - 1) % kSongCount;
    }
    if ((pressed & kBtnListen) && IsSongLearned(kSongs[sSel])) {
        PlaySelectedMelody();
    }

    DrawMenu();
}
