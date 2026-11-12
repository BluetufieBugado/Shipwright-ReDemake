#include <libultraship/bridge/consolevariablebridge.h>
#include "libultraship/libultra/controller.h"
#include <ship/Context.h>
#include <libultraship/controller/controldeck/ControlDeck.h>
#include <imgui.h>
#include <cmath>
#include <vector>

#include "OcarinaSongMenu.h"
#include "soh/OTRGlobals.h"

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

namespace {

#define CVAR_OCA_MENU_NAME CVAR_ENHANCEMENT("OcarinaSongMenu")

// Botões de controle usados pelo menu (mude aqui se quiser outro mapeamento)
constexpr int kBtnToggle = BTN_DDOWN; // abre/fecha o menu
constexpr int kBtnPrev = BTN_DLEFT;
constexpr int kBtnNext = BTN_DRIGHT;
constexpr int kBtnListen = BTN_Z; // ouvir a melodia

struct SongInfo {
    const char* name;
    const char* desc;
    int songId;        // OCARINA_SONG_*
    int questItem;     // QUEST_SONG_*
    ImU32 color;       // cor da bolinha (placeholder até existirem ícones PNG)
    const char* glyph; // sigla curta dentro da bolinha (placeholder)
};

const SongInfo kSongs[] = {
    { "Zelda's Lullaby", "A gentle melody for the royal family.", OCARINA_SONG_LULLABY, QUEST_SONG_LULLABY,
      IM_COL32(230, 200, 90, 255), "Z" },
    { "Epona's Song", "A melody that calls your horse.", OCARINA_SONG_EPONAS, QUEST_SONG_EPONA,
      IM_COL32(200, 130, 80, 255), "E" },
    { "Saria's Song", "A cheerful tune from a friend in the forest.", OCARINA_SONG_SARIAS, QUEST_SONG_SARIA,
      IM_COL32(90, 190, 90, 255), "S" },
    { "Sun's Song", "Turns day to night, and night to day.", OCARINA_SONG_SUNS, QUEST_SONG_SUN,
      IM_COL32(240, 190, 60, 255), "So" },
    { "Song of Time", "Opens the Door of Time.", OCARINA_SONG_TIME, QUEST_SONG_TIME, IM_COL32(110, 150, 230, 255),
      "T" },
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

void DrawNoteBlock(ImDrawList* dl, ImVec2 c, BtnNote n) {
    const char* labels[] = { "A", "v", ">", "<", "^" };
    ImU32 bg = (n == BN_A) ? IM_COL32(60, 90, 220, 255) : IM_COL32(230, 190, 40, 255);
    ImU32 fg = (n == BN_A) ? IM_COL32_WHITE : IM_COL32(40, 30, 10, 255);
    dl->AddRectFilled(ImVec2(c.x - 13, c.y - 11), ImVec2(c.x + 13, c.y + 11), bg, 4.0f);
    dl->AddRect(ImVec2(c.x - 13, c.y - 11), ImVec2(c.x + 13, c.y + 11), IM_COL32(40, 30, 20, 255), 4.0f, 0, 1.5f);
    ImVec2 ts = ImGui::CalcTextSize(labels[n]);
    dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), fg, labels[n]);
}

void DrawMenu() {
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
        const float gap = (pMax.y - pMin.y) * 0.085f;
        const float left = pMin.x + (pMax.x - pMin.x) * 0.10f;
        const float right = pMax.x - (pMax.x - pMin.x) * 0.06f;
        const float staffTop = pMin.y + (pMax.y - pMin.y) * 0.30f;
        for (int l = 0; l < 5; l++) {
            dl->AddLine(ImVec2(left, staffTop + (float)l * gap), ImVec2(right, staffTop + (float)l * gap),
                        IM_COL32(70, 48, 28, 255), 1.5f);
        }
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
    bool enabled = CVarGetInteger(CVAR_OCA_MENU_NAME, 0) != 0;
    bool ocarinaActive = enabled && IsGameplayReady() && AudioOcarina_IsActive() != 0;
    if (!ocarinaActive) {
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
