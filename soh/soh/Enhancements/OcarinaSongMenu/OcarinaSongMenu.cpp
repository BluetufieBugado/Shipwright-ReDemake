#include "OcarinaSongMenu.h"
#include <imgui.h>
#include <cmath>
#include <vector>

#include "soh/OTRGlobals.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
#include "functions.h"
extern PlayState* gPlayState;
}

namespace {

#define CVAR_OCA_MENU_NAME CVAR_ENHANCEMENT("OcarinaSongMenu")

struct SongInfo {
    const char* name;
    const char* desc;
    int songId;       // OCARINA_SONG_*
    int questItem;    // QUEST_SONG_*
    ImU32 color;      // cor da bolinha (placeholder até existirem ícones PNG)
    const char* glyph; // símbolo curto dentro da bolinha (placeholder)
};

// Ordem de exibição (cima para baixo, esquerda para direita)
const SongInfo kSongs[] = {
    { "Zelda's Lullaby", "A gentle melody for the royal family.", OCARINA_SONG_LULLABY, QUEST_SONG_LULLABY,
      IM_COL32(230, 200, 90, 255), "Z" },
    { "Epona's Song", "A melody that calls your horse.", OCARINA_SONG_EPONAS, QUEST_SONG_EPONA,
      IM_COL32(200, 130, 80, 255), "E" },
    { "Saria's Song", "A cheerful tune from a friend in the forest.", OCARINA_SONG_SARIAS, QUEST_SONG_SARIA,
      IM_COL32(90, 190, 90, 255), "S" },
    { "Sun's Song", "Turns day to night, and night to day.", OCARINA_SONG_SUNS, QUEST_SONG_SUN,
      IM_COL32(240, 190, 60, 255), "So" },
    { "Song of Time", "Opens the Door of Time.", OCARINA_SONG_TIME, QUEST_SONG_TIME,
      IM_COL32(110, 150, 230, 255), "T" },
    { "Song of Storms", "Calls forth rain and storms.", OCARINA_SONG_STORMS, QUEST_SONG_STORMS,
      IM_COL32(150, 150, 200, 255), "St" },
    { "Minuet of Forest", "This melody will take you to the Sacred Forest Meadow.", OCARINA_SONG_MINUET,
      QUEST_SONG_MINUET, IM_COL32(60, 170, 80, 255), "Mi" },
    { "Bolero of Fire", "This melody will take you to Death Mountain Crater.", OCARINA_SONG_BOLERO, QUEST_SONG_BOLERO,
      IM_COL32(220, 70, 50, 255), "Bo" },
    { "Serenade of Water", "This whimsical melody will take you to Lake Hylia.", OCARINA_SONG_SERENADE,
      QUEST_SONG_SERENADE, IM_COL32(70, 110, 230, 255), "Se" },
    { "Requiem of Spirit", "This melody will take you to the Desert Colossus.", OCARINA_SONG_REQUIEM,
      QUEST_SONG_REQUIEM, IM_COL32(230, 150, 60, 255), "Re" },
    { "Nocturne of Shadow", "This melody will take you to the Graveyard.", OCARINA_SONG_NOCTURNE, QUEST_SONG_NOCTURNE,
      IM_COL32(150, 60, 170, 255), "No" },
    { "Prelude of Light", "This melody will take you to the Temple of Time.", OCARINA_SONG_PRELUDE, QUEST_SONG_PRELUDE,
      IM_COL32(240, 230, 130, 255), "Pr" },
};
constexpr int kSongCount = (int)(sizeof(kSongs) / sizeof(kSongs[0]));

enum BtnNote { BN_A = 0, BN_DOWN, BN_RIGHT, BN_LEFT, BN_UP, BN_NONE };

// Sequencia de botoes de cada musica (A, Baixo, Direita, Esquerda, Cima).
// Fixa no codigo para nao depender de simbolos internos do jogo.
std::vector<BtnNote> GetSongButtons(int songId) {
    switch (songId) {
        case OCARINA_SONG_LULLABY:
            return { BN_LEFT, BN_UP, BN_RIGHT, BN_LEFT, BN_UP, BN_RIGHT };
        case OCARINA_SONG_EPONAS:
            return { BN_UP, BN_LEFT, BN_RIGHT, BN_UP, BN_LEFT, BN_RIGHT };
        case OCARINA_SONG_SARIAS:
            return { BN_DOWN, BN_RIGHT, BN_LEFT, BN_DOWN, BN_RIGHT, BN_LEFT };
        case OCARINA_SONG_SUNS:
            return { BN_RIGHT, BN_DOWN, BN_UP, BN_RIGHT, BN_DOWN, BN_UP };
        case OCARINA_SONG_TIME:
            return { BN_RIGHT, BN_A, BN_DOWN, BN_RIGHT, BN_A, BN_DOWN };
        case OCARINA_SONG_STORMS:
            return { BN_A, BN_DOWN, BN_UP, BN_A, BN_DOWN, BN_UP };
        case OCARINA_SONG_MINUET:
            return { BN_A, BN_UP, BN_LEFT, BN_RIGHT, BN_LEFT, BN_RIGHT };
        case OCARINA_SONG_BOLERO:
            return { BN_DOWN, BN_A, BN_DOWN, BN_A, BN_RIGHT, BN_DOWN, BN_RIGHT, BN_DOWN };
        case OCARINA_SONG_SERENADE:
            return { BN_A, BN_DOWN, BN_RIGHT, BN_RIGHT, BN_LEFT };
        case OCARINA_SONG_REQUIEM:
            return { BN_A, BN_DOWN, BN_A, BN_RIGHT, BN_DOWN, BN_A };
        case OCARINA_SONG_NOCTURNE:
            return { BN_LEFT, BN_RIGHT, BN_RIGHT, BN_A, BN_LEFT, BN_RIGHT, BN_DOWN };
        case OCARINA_SONG_PRELUDE:
            return { BN_UP, BN_RIGHT, BN_UP, BN_RIGHT, BN_LEFT, BN_UP };
        default:
            return {};
    }
}

bool sOpen = false;
int sSel = 0;

bool IsSongLearned(const SongInfo& s) {
    return CHECK_QUEST_ITEM(s.questItem);
}

bool IsReadyToShow() {
    return gPlayState != nullptr && gSaveContext.gameMode == 0 && gPlayState->pauseCtx.state == 0;
}

void DrawNoteBlock(ImDrawList* dl, ImVec2 c, BtnNote n) {
    const char* labels[] = { "A", "v", ">", "<", "^" };
    ImU32 bg = (n == BN_A) ? IM_COL32(60, 90, 220, 255) : IM_COL32(230, 190, 40, 255);
    ImU32 fg = (n == BN_A) ? IM_COL32_WHITE : IM_COL32(40, 30, 10, 255);
    dl->AddRectFilled(ImVec2(c.x - 13, c.y - 11), ImVec2(c.x + 13, c.y + 11), bg, 4.0f);
    dl->AddRect(ImVec2(c.x - 13, c.y - 11), ImVec2(c.x + 13, c.y + 11), IM_COL32(40, 30, 20, 255), 4.0f, 0, 1.5f);
    ImVec2 ts = ImGui::CalcTextSize(labels[n]);
    dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), fg, labels[n]);
}

void PlaySelectedMelody() {
    // +1 porque a função recebe "índice + 1" (0 = parar)
    AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_DEFAULT);
    AudioOcarina_SetPlaybackSong(kSongs[sSel].songId + 1, 1);
}

} // namespace

void OcarinaSongMenuWindow::InitElement() {
}

void OcarinaSongMenuWindow::DrawElement() {
    if (!CVarGetInteger(CVAR_OCA_MENU_NAME, 0) || !IsReadyToShow()) {
        sOpen = false;
        return;
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGuiIO& io = ImGui::GetIO();

    // ---- Botão fixo inferior ----
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y - 12), ImGuiCond_Always,
                            ImVec2(0.5f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("##OcarinaSongMenuBtn", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
    if (ImGui::Button(sOpen ? "Close Songs" : "Songs", ImVec2(140, 36))) {
        sOpen = !sOpen;
        if (!sOpen) {
            AudioOcarina_SetPlaybackSong(0, 0);
        }
    }
    ImGui::End();

    if (!sOpen) {
        return;
    }

    // ---- Navegação (teclado/gamepad) ----
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight)) {
        sSel = (sSel + 1) % kSongCount;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft)) {
        sSel = (sSel + kSongCount - 1) % kSongCount;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown)) {
        if (IsSongLearned(kSongs[sSel])) {
            PlaySelectedMelody();
        }
    }

    // ---- Overlay de desenho ----
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::Begin("##OcarinaSongMenuRing", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float W = vp->Size.x, H = vp->Size.y;
    const float r = fminf(W, H) * 0.045f;
    const float cx = vp->Pos.x + W * 0.5f;

    // Duas fileiras em arco, como no remake
    const int topCount = (kSongCount + 1) / 2;
    const int botCount = kSongCount - topCount;
    for (int i = 0; i < kSongCount; i++) {
        bool top = i < topCount;
        int k = top ? i : i - topCount;
        int cnt = top ? topCount : botCount;
        float t = (cnt > 1) ? (float)k / (float)(cnt - 1) : 0.5f; // 0..1
        float radX = W * (top ? 0.30f : 0.24f);
        float radY = H * (top ? 0.17f : 0.10f);
        ImVec2 c(cx + (t - 0.5f) * 2.0f * radX, vp->Pos.y + H * (top ? 0.30f : 0.42f) - sinf(3.14159f * t) * radY);

        bool learned = IsSongLearned(kSongs[i]);
        dl->AddCircleFilled(c, r, learned ? IM_COL32(25, 25, 30, 235) : IM_COL32(110, 110, 110, 150));
        if (learned) {
            dl->AddCircleFilled(c, r * 0.62f, kSongs[i].color);
            ImVec2 ts = ImGui::CalcTextSize(kSongs[i].glyph);
            dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), IM_COL32(20, 20, 20, 255), kSongs[i].glyph);
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
    ImVec2 pMin(vp->Pos.x + W * 0.12f, vp->Pos.y + H * 0.58f);
    ImVec2 pMax(vp->Pos.x + W * 0.88f, vp->Pos.y + H * 0.86f);
    dl->AddRectFilled(pMin, pMax, IM_COL32(224, 202, 152, 250), 10.0f);
    dl->AddRect(pMin, pMax, IM_COL32(150, 108, 58, 255), 10.0f, 0, 4.0f);

    const char* title = learned ? s.name : "???";
    ImVec2 tsz = ImGui::CalcTextSize(title);
    dl->AddText(ImVec2((pMin.x + pMax.x - tsz.x) * 0.5f, pMin.y + 12), IM_COL32(70, 48, 28, 255), title);

    if (learned) {
        // Pauta: 5 linhas
        const float gap = (pMax.y - pMin.y) * 0.085f;
        const float left = pMin.x + (pMax.x - pMin.x) * 0.10f;
        const float right = pMax.x - (pMax.x - pMin.x) * 0.06f;
        const float top = pMin.y + (pMax.y - pMin.y) * 0.30f;
        for (int l = 0; l < 5; l++) {
            dl->AddLine(ImVec2(left, top + l * gap), ImVec2(right, top + l * gap), IM_COL32(70, 48, 28, 255), 1.5f);
        }
        // Notas: mais agudo = mais alto. Ordem de altura: A < v < > < < < ^
        std::vector<BtnNote> notes = GetSongButtons(s.songId);
        float step = fminf(46.0f, (right - left - 90.0f) / fmaxf(1.0f, (float)notes.size()));
        for (size_t i = 0; i < notes.size(); i++) {
            float x = left + 70.0f + (float)i * step;
            // A (mais grave) na linha de baixo, C-Cima (mais agudo) na linha de cima
            float y = top + gap * (4.0f - (float)notes[i]);
            DrawNoteBlock(dl, ImVec2(x, y), notes[i]);
        }
        ImVec2 dsz = ImGui::CalcTextSize(s.desc);
        dl->AddText(ImVec2((pMin.x + pMax.x - dsz.x) * 0.5f, pMax.y - 34), IM_COL32(70, 48, 28, 255), s.desc);
    } else {
        const char* msg = "You haven't learned this song yet.";
        ImVec2 msz = ImGui::CalcTextSize(msg);
        dl->AddText(ImVec2((pMin.x + pMax.x - msz.x) * 0.5f, (pMin.y + pMax.y) * 0.5f), IM_COL32(70, 48, 28, 255), msg);
    }

    // ---- Dicas de botão ----
    const char* hint = "Left/Right (D-Pad): select song      Enter / A: listen to the melody";
    ImVec2 hsz = ImGui::CalcTextSize(hint);
    dl->AddText(ImVec2((pMin.x + pMax.x - hsz.x) * 0.5f, pMax.y + 10), IM_COL32_WHITE, hint);

    ImGui::End();
}
